#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <exception>


template <typename Object>
struct Change {
    virtual bool apply(Object&) = 0;
    virtual ~Change() = default;
};


template <typename Object, typename Field>
class FieldAssignmentChange : public Change<Object> {
    Field Object::*member;
    Field value;

public:
    FieldAssignmentChange(Field Object::*m, Field v)
        : member(m), value(std::move(v)) {}

    bool apply(Object& obj) override {
        obj.*member = value;
        return true;
    }
};


template <typename Object, typename Arg>
class BoolMethodChange : public Change<Object> {
    bool (Object::*method)(Arg);
    Arg value;

public:
    BoolMethodChange(bool (Object::*m)(Arg), Arg v)
        : method(m), value(std::move(v)) {}

    bool apply(Object& obj) override {
        return (obj.*method)(value);
    }
};


template <typename Object, typename Arg>
class VoidMethodChange : public Change<Object> {
    void (Object::*method)(Arg);
    Arg value;

public:
    VoidMethodChange(void (Object::*m)(Arg), Arg v)
        : method(m), value(std::move(v)) {}

    bool apply(Object& obj) override {
        try {
            (obj.*method)(value);
            return true;
        } catch (...) {
            return false;
        }
    }
};


template <typename Object, typename Field>
class FieldProxy {
    Field Object::*member;

public:
    explicit FieldProxy(Field Object::*m) : member(m) {}

    std::unique_ptr<Change<Object>> operator=(Field value) const {
        return std::make_unique<FieldAssignmentChange<Object, Field>>(member, std::move(value));
    }
};


template <typename Object, typename Arg>
class BoolMethodProxy {
    bool (Object::*method)(Arg);

public:
    explicit BoolMethodProxy(bool (Object::*m)(Arg)) : method(m) {}

    std::unique_ptr<Change<Object>> operator=(Arg value) const {
        return std::make_unique<BoolMethodChange<Object, Arg>>(method, std::move(value));
    }
};


template <typename Object, typename Arg>
class VoidMethodProxy {
    void (Object::*method)(Arg);

public:
    explicit VoidMethodProxy(void (Object::*m)(Arg)) : method(m) {}

    std::unique_ptr<Change<Object>> operator=(Arg value) const {
        return std::make_unique<VoidMethodChange<Object, Arg>>(method, std::move(value));
    }
};


template <typename Object>
class UpdateSet {
    std::vector<std::unique_ptr<Change<Object>>> changes;

public:
    void add(std::unique_ptr<Change<Object>> c) {
        changes.emplace_back(std::move(c));
    }

    const auto& get() const {
        return changes;
    }
};


template <typename Object>
class AtomicTransaction {
    Object& original;
    Object copy;

public:
    explicit AtomicTransaction(Object& obj)
        : original(obj), copy(obj) {}

    bool execute(const UpdateSet<Object>& updates) {
        for (const auto& c : updates.get()) {
            if (!c->apply(copy))
                return false;
        }
        original = std::move(copy);
        return true;
    }
};



template <typename Object>
class AtomicChangeProxy {
    Object& obj;

public:
    explicit AtomicChangeProxy(Object& o) : obj(o) {}

    template <typename... Changes>
    bool operator()(Changes&&... changes) {
        UpdateSet<Object> set;
        (set.add(std::forward<Changes>(changes)), ...);

        AtomicTransaction<Object> tx(obj);
        return tx.execute(set);
    }
};

template <typename Object>
AtomicChangeProxy<Object> atomic_change_to(Object& obj) {
    return AtomicChangeProxy<Object>(obj);
}



struct Record {
    bool setName(std::string str) {
        if (str.empty()) return false;
        name = std::move(str);
        return true;
    }

    void age(int value) {
        if (value < 0) throw std::runtime_error("Invalid age");
        _age = value;
    }

    std::string position;

private:
    int _age{};
    std::string name;

public:
    void dump() const {
        std::cout << "name=" << name
                  << " age=" << _age
                  << " position=" << position << '\n';
    }
};

int main() {
    Record person;

    FieldProxy<Record, std::string> position(&Record::position);
    BoolMethodProxy<Record, std::string> name(&Record::setName);
    VoidMethodProxy<Record, int> age(&Record::age);

    bool success = atomic_change_to(person)(
        name = "",
        age = 22,
        position = "Best Developer"
    );

    std::cout << "success = " << success << "\n";
    person.dump();
}
