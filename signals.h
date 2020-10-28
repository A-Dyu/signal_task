#pragma once
#include <functional>
#include "intrusive_list.h"

namespace signals
{

template <typename T>
struct signal;

template <typename... Args>
struct signal<void (Args...)>
{
    using slot_t = std::function<void (Args...)>;

    struct connection_tag;

    struct connection : intrusive::list_element<connection_tag> {
        connection() = default;

        connection(signal* sig, slot_t&& slot) : slot(std::move(slot)), sig(sig) {}

        connection(connection&& other): connection() {
            replace_connection(other);
        }

        connection& operator=(connection&& other) {
            if (&other != this) {
                disconnect();
                replace_connection(other);
            }
            return *this;
        }

       ~connection() {
            disconnect();
        }

        void disconnect() {
            if (this->is_linked()) {
                for (iteration_token *token = sig->top_token; token; token = token->next) {
                    if (token->current != sig->connections.end() && &*token->current == this) {
                        token->current++;
                    }
                }
                clear();
            }
        }

        friend struct signal;

    private:
        void replace_connection(connection& other) {
            slot = std::move(other.slot);
            sig = other.sig;
            if (other.is_linked()) {
                auto it = sig->connections.get_iterator(other);
                ++it;
                sig->connections.insert(it, *this);
                other.disconnect();
            }
        }

        void clear() noexcept {
            this->unlink();
            slot = {};
            sig = nullptr;
        }

        slot_t slot;
        signal* sig;
    };

    using connections_t = intrusive::list<connection, connection_tag>;

    struct iteration_token {
        explicit iteration_token(signal const* sig) : current(sig->connections.begin()), next(sig->top_token), sig(sig) {
            sig->top_token = this;
        }

        bool is_destroyed() const noexcept {
            return sig == nullptr;
        }

        void set_destroyed() noexcept {
            sig = nullptr;
        }

        ~iteration_token() {
            if (!is_destroyed()) {
                sig->top_token = next;
            }
        }

        typename connections_t::const_iterator current;
        iteration_token* next;
        signal const* sig;
    };

    signal() = default;

    signal(signal const&) = delete;
    signal& operator=(signal const&) = delete;

    ~signal() {
        for (iteration_token* tok = top_token; tok; tok = tok->next) {
            tok->set_destroyed();
        }
        while (!connections.empty()) {
            connections.front().clear();
        }
    };

    connection connect(std::function<void (Args...)> slot) noexcept{
        connection con(this, std::move(slot));
        connections.insert(connections.end(), con);
        return con;
    }

    void operator()(Args... args) const {
        iteration_token token(this);
        while (token.current != connections.end()) {
            auto safe = token.current;
            token.current++;
            safe->slot(args...);
            if (token.is_destroyed()) {
                return;
            }
        }
    }

private:
    connections_t connections;
    mutable iteration_token* top_token;
};

}
