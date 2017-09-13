// EDIT THIS FILE. Follow the instructions in Exercise 3 to turn it from a copy of "function.h"
// into a valid implementation of std::any.

#include <memory>
#include <typeinfo>
#include <type_traits>
#include <utility>

template<class Signature> class function;  // For example, you should modify this line to just "class any;"
template<class Signature> struct ContainerBase;
template<class Signature, class Wrapped> struct Container;

// Boolean type-trait for "Is this type a std::function type?"
template<class T> struct is_specialization_of_function : std::false_type {};
template<class Signature> struct is_specialization_of_function<function<Signature>> : std::true_type {};
template<class T> inline constexpr bool is_specialization_of_function_v = is_specialization_of_function<T>::value;

// Partial specialization of ContainerBase.
template<class R, class... A>
struct ContainerBase<R(A...)> {
    virtual void copy_to(function<R(A...)>&) const = 0;
    virtual const std::type_info& type() const = 0;
    virtual void *data() const = 0;
    virtual R callme(A...) = 0;
    virtual ~ContainerBase() = default;
};

// Partial specialization of Container.
template<class Wrapped, class R, class... A>
struct Container<Wrapped, R(A...)> : ContainerBase<R(A...)> {
    void copy_to(function<R(A...)>&) const override;
    const std::type_info& type() const override { return typeid(Wrapped); }
    void *data() const override { return (void *)(&m_data); }
    R callme(A... args) override { return m_data(std::forward<A>(args)...); }

    template<typename... Args>
    Container(Args&&... args) : m_data(std::forward<Args>(args)...) {}
private:
    Wrapped m_data;
};

template<class R, class... A>
class function<R(A...)> {
    std::unique_ptr<ContainerBase<R(A...)>> m_ptr;
public:
    // The special member functions.

    constexpr function() noexcept = default;

    function(const function& rhs) {
        if (rhs.has_value()) {
            rhs.m_ptr->copy_to(*this);
        }
    }
    function(function&& rhs) {
        m_ptr = std::move(rhs.m_ptr);
    }

    function& operator=(const function& rhs) {
        function(rhs).swap(*this);
        return *this;
    }
    function& operator=(function&& rhs) {
        function(std::move(rhs)).swap(*this);
        return *this;
    }

    template<class T, class DT = std::decay_t<T>,
        class = std::enable_if_t<!std::is_same_v<DT, function>>>
    function(T&& value) {
        if constexpr (std::is_pointer_v<DT> || is_specialization_of_function_v<DT>) {
            // Constructing from a null function pointer or an empty std::function object,
            // of whatever type, should produce a valueless (default-constructed) std::function.
            if (!value) return;
        }
        this->emplace<DT>(std::forward<T>(value));
    }
    function(std::nullptr_t) {}

    template<class T, class DT = std::decay_t<T>,
        class = std::enable_if_t<!std::is_same_v<DT, function>>>
    function& operator=(T&& value) {
        function(std::forward<T>(value)).swap(*this);
        return *this;
    }

    ~function() = default;

    // The primitive operations.

    template<class Wrapped, class... Args>
    Wrapped& emplace(Args&&... args) {
        m_ptr = std::make_unique<Container<Wrapped, R(A...)>>(std::forward<Args>(args)...);
        return *static_cast<Wrapped*>(m_ptr->data());
    }
    void reset() noexcept {
        m_ptr = nullptr;
    }
    void swap(function& rhs) noexcept {
        m_ptr.swap(rhs.m_ptr);
    }
    bool has_value() const noexcept {
        return (m_ptr != nullptr);
    }
    explicit operator bool() const noexcept {
        return has_value();
    }
    R operator()(A... args) {
        return m_ptr->callme(std::forward<A>(args)...);
    }

    // Type-unerasure.

    const std::type_info& target_type() const noexcept {
        return m_ptr ? m_ptr->type() : typeid(void);
    }
    template<class T>
    T* target() noexcept {
        return (m_ptr && m_ptr->type() == typeid(T)) ? static_cast<T*>(m_ptr->data()) : nullptr;
    }
    template<class T>
    const T* target() const noexcept {
        return (m_ptr && m_ptr->type() == typeid(T)) ? static_cast<const T*>(m_ptr->data()) : nullptr;
    }
};

// This is down here only so that the definition of "emplace" will have already been seen.
template<class Wrapped, class R, class... A>
void Container<Wrapped, R(A...)>::copy_to(function<R(A...)>& destination) const
{
    destination.template emplace<Wrapped>(m_data);
}
