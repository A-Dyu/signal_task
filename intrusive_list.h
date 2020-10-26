#pragma once
#include <cassert>
#include <iterator>
#include <type_traits>

/*
Я разместил всё в неймспейсе, чтобы подсократить имена:
intrusive_list         -> list
intrusive_list_element -> list_element
default_intrusive_tag  -> default_tag.
*/
namespace intrusive
{
    struct default_tag;

    struct list_element_base
    {
        void unlink() noexcept;
        void try_unlink() noexcept;
        void clear() noexcept;
        void insert(list_element_base&) noexcept;
        void splice(list_element_base& first, list_element_base& last) noexcept;
        bool is_linked() const noexcept;

        list_element_base* prev;
        list_element_base* next;
    };

    template <typename Tag = default_tag>
    struct list_element : private list_element_base
    {
        /*
        Вся содержательная функциональность вынесена в
        нешаблонную базу, чтобы не дублировался код для
        каждого отдельного тега (Tag).
        База private и считается деталью реализации.
        */

        /*
        В этой реализации list_element автоматически
        отвязывается от списка, при удалении. ИМХО это самое
        простое в использовании поведение. В терминах
        Boost.Intrusive это называется "Auto-unlink hook".
        https://www.boost.org/doc/libs/1_74_0/doc/html/intrusive/auto_unlink_hooks.html
        */

        list_element() noexcept;
        ~list_element() noexcept;
        list_element(list_element const&) = delete;
        list_element& operator=(list_element const&) = delete;

        /*
        unlink() вытащен в public интерфейс так же как в Boost.Intrusive.
        */
        using list_element_base::unlink;

        using list_element_base::is_linked;

        template <typename T, typename Tag1>
        friend struct list;

        template <typename Tag1, typename T>
        friend list_element_base& to_base(T&) noexcept;

        template <typename Tag1, typename T>
        friend list_element_base const& to_base(T const&) noexcept;

        template <typename T1, typename Tag1>
        friend T1& from_base(list_element_base&) noexcept;

        template <typename T1, typename Tag1>
        friend T1 const& from_base(list_element_base const&) noexcept;
    };

    template <typename T, typename Tag>
    struct list_iterator
    {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::remove_const_t<T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        list_iterator() = default;
        template <typename NonConstIterator>
        list_iterator(NonConstIterator other,
                      std::enable_if_t<
                              std::is_same_v<NonConstIterator, list_iterator<std::remove_const_t<T>, Tag>> &&
                              std::is_const_v<T>>* = nullptr) noexcept
                : current(other.current)
        {}

        T& operator*() const noexcept;
        T* operator->() const noexcept;

        list_iterator& operator++() & noexcept;
        list_iterator& operator--() & noexcept;

        list_iterator operator++(int) & noexcept;
        list_iterator operator--(int) & noexcept;

        bool operator==(list_iterator const& rhs) const& noexcept;
        bool operator!=(list_iterator const& rhs) const& noexcept;

    private:
        /*
        Это важно иметь этот конструктор private, чтобы итератор нельзя было создать
        от nullptr.
        */
        explicit list_iterator(list_element_base* current) noexcept;

    private:
        /*
        Хранить list_element_base*, а не T* важно.
        Иначе нельзя будет создать list_iterator для
        end().
        */
        list_element_base* current;

        template <typename T1, typename Tag1>
        friend struct list_iterator;

        template <typename T1, typename Tag1>
        friend struct list;
    };

    /*
    Вспомогательные функции, чтобы не писать static_cast'ы повсюду.
    Перегрузки от const и не-const упрощают реализацию итераторов.
    В to_base параметр T может быть выведен, поэтому я поставил
    его в конец. Из-за этого to_base имеет не тот порядок параметров
    как все остальные функции и классы. Это не очень хорошо. Может
    быть забить и сделать порядок параметров как везде. Я не знаю.
    */
    template <typename Tag, typename T>
    list_element_base& to_base(T&) noexcept;

    template <typename Tag, typename T>
    list_element_base const& to_base(T const&) noexcept;

    template <typename T, typename Tag>
    T& from_base(list_element_base&) noexcept;

    template <typename T, typename Tag>
    T const& from_base(list_element_base const&) noexcept;

    template <typename T, typename Tag = default_tag>
    struct list
    {
        using iterator = list_iterator<T, Tag>;
        using const_iterator = list_iterator<T const, Tag>;

        /*
        Я не рассказывал на лекции про static_assert. Можно рассказать
        им про него на практике.
        */
        static_assert(std::is_convertible_v<T&, list_element<Tag>&>,
                      "value type is not convertible to list_element");

        /*
        Практически все операции получились noexcept, поскольку мы нигде не
        аллоцируем память и не вызываем пользовательские функции.
        */
        list() noexcept;
        list(list const&) = delete;
        list(list&&) noexcept;
        ~list();

        list& operator=(list const&) = delete;
        list& operator=(list&&) noexcept;

        void clear() noexcept;

        /*
        Реализуя интрузивный список, я вижу два способа предоставлять
        интерфейс обычного списка. Можно делать вид, что наш
        value_type это T*. Но тогда работа с итераторами становиться
        не очень красивой (*it)->member. Другой способ это делать вид
        что наш value_type как-бы T&. Я использую второй способ.
        */

        /*
        Поскольку вставка изменяет данные в list_element
        мы принимаем неконстантный T&.
        */
        void push_back(T&) noexcept;
        void pop_back() noexcept;
        T& back() noexcept;
        T const& back() const noexcept;

        void push_front(T&) noexcept;
        void pop_front() noexcept;
        T& front() noexcept;
        T const& front() const noexcept;

        bool empty() const noexcept;

        iterator begin() noexcept;
        const_iterator begin() const noexcept;

        iterator end() noexcept;
        const_iterator end() const noexcept;

        iterator insert(const_iterator pos, T&) noexcept;
        iterator erase(const_iterator pos) noexcept;
        void splice(const_iterator pos, list&, const_iterator first, const_iterator last) noexcept;

        iterator get_iterator(T& element) noexcept {
            return iterator(static_cast<list_element<Tag>*>(&element));
        }

        const_iterator get_iterator(T& element) const noexcept {
            return const_iterator(static_cast<list_element<Tag>*>(&element));
        }

    private:
        mutable list_element_base fake;
    };
}

template <typename Tag>
intrusive::list_element<Tag>::list_element() noexcept
        : list_element_base{nullptr, nullptr}
{}

template <typename Tag>
intrusive::list_element<Tag>::~list_element() noexcept
{
    try_unlink();
}

template <typename T, typename Tag>
T& intrusive::list_iterator<T, Tag>::operator*() const noexcept
{
    return from_base<T, Tag>(*current);
}

template <typename T, typename Tag>
T* intrusive::list_iterator<T, Tag>::operator->() const noexcept
{
    return &from_base<T, Tag>(*current);
}

template <typename T, typename Tag>
intrusive::list_iterator<T, Tag>& intrusive::list_iterator<T, Tag>::operator++() & noexcept
{
    current = current->next;
    return *this;
}

template <typename T, typename Tag>
intrusive::list_iterator<T, Tag>& intrusive::list_iterator<T, Tag>::operator--() & noexcept
{
    current = current->prev;
    return *this;
}

template <typename T, typename Tag>
intrusive::list_iterator<T, Tag> intrusive::list_iterator<T, Tag>::operator++(int) & noexcept
{
    list_iterator copy = *this;
    ++*this;
    return copy;
}

template <typename T, typename Tag>
intrusive::list_iterator<T, Tag> intrusive::list_iterator<T, Tag>::operator--(int) & noexcept
{
    list_iterator copy = *this;
    --*this;
    return copy;
}

template <typename T, typename Tag>
bool intrusive::list_iterator<T, Tag>::operator==(list_iterator const& rhs) const& noexcept
{
    return current == rhs.current;
}

template <typename T, typename Tag>
bool intrusive::list_iterator<T, Tag>::operator!=(list_iterator const& rhs) const& noexcept
{
    return current != rhs.current;
}

template <typename T, typename Tag>
intrusive::list_iterator<T, Tag>::list_iterator(list_element_base* current) noexcept
        : current(current)
{}

template <typename Tag, typename T>
intrusive::list_element_base& intrusive::to_base(T& obj) noexcept
{
    return static_cast<list_element<Tag>&>(obj);
}

template <typename Tag, typename T>
intrusive::list_element_base const& intrusive::to_base(T const& obj) noexcept
{
    return static_cast<list_element<Tag> const&>(obj);
}

template <typename T, typename Tag>
T& intrusive::from_base(list_element_base& base) noexcept
{
    return static_cast<T&>(static_cast<list_element<Tag>&>(base));
}

template <typename T, typename Tag>
T const& intrusive::from_base(list_element_base const& base) noexcept
{
    return static_cast<T const&>(static_cast<list_element<Tag> const&>(base));
}

template <typename Node, typename Tag>
intrusive::list<Node, Tag>::list() noexcept
        : fake{&fake, &fake}
{}

template <typename T, typename Tag>
intrusive::list<T, Tag>::list(list&& other) noexcept
        : list()
{
    /*
    Для intrusive_list'а реализация move'а и swap'а тонкая:
    нам надо не только поменять fake.{prev,next}, но ещё и
    обновить ноды, которые ссылаются на fake.
    Я решил, что простейший способ сделать это для move'а --
    это вызвать splice.
    swap я не стал реализовывать решив что стандартный, делающий
    три move'а подойдет, но я не проверял. Может быть стоит
    написать swap ручками.
    */

    splice(end(), other, other.begin(), other.end());
}

template <typename Node, typename Tag>
intrusive::list<Node, Tag>::~list()
{
    fake.clear();
}

template <typename T, typename Tag>
intrusive::list<T, Tag>& intrusive::list<T, Tag>::operator=(list&& other) noexcept
{
    fake.clear();
    splice(end(), other, other.begin(), other.end());
    return *this;
}

template <typename Node, typename Tag>
void intrusive::list<Node, Tag>::clear() noexcept
{
    fake.clear();
}

template <typename T, typename Tag>
void intrusive::list<T, Tag>::push_back(T& obj) noexcept
{
    fake.insert(to_base<Tag>(obj));
}

template <typename T, typename Tag>
void intrusive::list<T, Tag>::pop_back() noexcept
{
    fake.prev->unlink();
}

template <typename T, typename Tag>
T& intrusive::list<T, Tag>::back() noexcept
{
    return from_base<T, Tag>(*fake.prev);
}

template <typename T, typename Tag>
T const& intrusive::list<T, Tag>::back() const noexcept
{
    return from_base<T, Tag>(*fake.prev);
}

template <typename T, typename Tag>
void intrusive::list<T, Tag>::push_front(T& obj) noexcept
{
    fake.next->insert(to_base<Tag>(obj));
}

template <typename T, typename Tag>
void intrusive::list<T, Tag>::pop_front() noexcept
{
    fake.next->unlink();
}

template <typename T, typename Tag>
T& intrusive::list<T, Tag>::front() noexcept
{
    return from_base<T, Tag>(*fake.next);
}

template <typename T, typename Tag>
T const& intrusive::list<T, Tag>::front() const noexcept
{
    return from_base<T, Tag>(*fake.next);
}

template <typename T, typename Tag>
bool intrusive::list<T, Tag>::empty() const noexcept
{
    return fake.prev == &fake;
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::iterator intrusive::list<T, Tag>::begin() noexcept
{
    return iterator(fake.next);
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::const_iterator intrusive::list<T, Tag>::begin() const noexcept
{
    return const_iterator(fake.next);
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::iterator intrusive::list<T, Tag>::end() noexcept
{
    return iterator(&fake);
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::const_iterator intrusive::list<T, Tag>::end() const noexcept
{
    return const_iterator(&fake);
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::iterator intrusive::list<T, Tag>::insert(const_iterator pos, T& obj) noexcept
{
    list_element_base& base = to_base<Tag>(obj);
    pos.current->insert(base);
    return iterator(&base);
}

template <typename T, typename Tag>
typename intrusive::list<T, Tag>::iterator intrusive::list<T, Tag>::erase(const_iterator pos) noexcept
{
    list_element_base* next = pos.current->next;
    pos.current->unlink();
    return iterator(next);
}

template <typename T, typename Tag>
void intrusive::list<T, Tag>::splice(const_iterator pos, list&, const_iterator first, const_iterator last) noexcept
{
    pos.current->splice(*first.current, *last.current);
}