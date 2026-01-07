#include <utility>
#include <exception>
#include <string>
#include <iostream>

template <typename T>
class atomic_transaction {
    T& original_;
    T  copy_;
    bool success_ = true;

public:
    explicit atomic_transaction(T& obj)
        : original_(obj), copy_(obj) {}

    template <typename Operation>
    void apply(Operation&& op) {
        if (!success_) return;

        try {
            success_ = op(copy_);
        } catch (...) {
            success_ = false;
        }
    }

    bool commit() {
        if (success_) {
            original_ = std::move(copy_);
        }
        return success_;
    }
};



template <typename T>
auto atomic_change_to(T& object) {
    return [&object](auto&&... operations) {
        atomic_transaction<T> tx(object);
        (tx.apply(std::forward<decltype(operations)>(operations)), ...);
        return tx.commit();
    };
}

#define ATOMIC_FIELD(field, value) \
    [](auto& obj) -> bool { obj.field = value; return true; }

#define ATOMIC_METHOD_BOOL(method, value) \
    [](auto& obj) -> bool { return obj.method(value); }

#define ATOMIC_METHOD_VOID(method, value) \
    [](auto& obj) -> bool { obj.method(value); return true; }



struct Record {
    bool setName(const std::string& str) {
        if (str.empty()) return false;
        name = str;
        return true;
    }

    void age(int value) {
        if (value < 0) throw 1;
        _age = value;
    }

    std::string position;

private:
    int _age{};
    std::string name;
};



int main() {
    Record person;

    bool success = atomic_change_to(person)(
        ATOMIC_METHOD_BOOL(setName, std::string("")),
        ATOMIC_METHOD_VOID(age, 22),
        ATOMIC_FIELD(position, std::string("Best Developer"))
    );

    std::cout << std::boolalpha << success << '\n';
}
