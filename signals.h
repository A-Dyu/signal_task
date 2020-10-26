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

        connection(connection&& other): slot(std::move(other.slot)), sig(other.sig) {
            if (other.is_linked()) {
                other.insert(*this);
                other.disconnect();
            }
        }

        connection& operator=(connection&& other) {
            if (&other != this) {
                connection safe;
                swap(safe);
                swap(other);
            }
            return *this;
        }

       ~connection() {
            disconnect();
        }

        void disconnect() {
            if (this->is_linked()) {
                slot = {};
                for (iteration_token *token = sig->top_token; token; token = token->next) {
                    if (token->current != sig->connections.end() && &*token->current == this) {
                        token->current++;
                    }
                }
                this->unlink();
            }
        }

        void swap(connection& other) {
            using std::swap;
            intrusive::list_element<connection_tag>::swap(other);
            swap(slot, other.slot);
            swap(sig, other.sig);
        }

        slot_t slot;
        signal* sig;
    };

    using connections_t = intrusive::list<connection, connection_tag>;

    struct iteration_token {
        explicit iteration_token(signal const* sig) : current(sig->connections.begin()), next(sig->top_token), destroyed(false) {
            sig->top_token = this;
        }
        typename connections_t::const_iterator current;
        iteration_token* next;
        bool destroyed;
    };

    signal() = default;

    signal(signal const&) = delete;
    signal& operator=(signal const&) = delete;

    ~signal() {
        while (!connections.empty()) {
            connections.front().disconnect();
        }
        for (iteration_token* tok = top_token; tok; tok = tok->next) {
            tok->destroyed = true;
        }
    };

    connection connect(std::function<void (Args...)> slot) noexcept{
        connection con(this, std::move(slot));
        connections.insert(connections.end(), con);
        return con;
    }

    void operator()(Args... args) const {
        iteration_token token(this);
        try {
            while (token.current != connections.end()) {
                auto safe = token.current;
                token.current++;
                safe->slot(args...);
                if (token.destroyed) {
                    return;
                }
            }
        } catch(...) {
            top_token = token.next;
            throw;
        }
        top_token = token.next;
    }

private:
    connections_t connections;
    mutable iteration_token* top_token;
};

}
