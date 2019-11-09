//
//  Variant.h
//  CppHelpers
//
//  Created by Sumant Hanumante on 9/21/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

#include <array>
#include <type_traits>
#include <utility>

namespace sh {
// Can be improved using
// https://ldionne.com/2015/11/29/efficient-parameter-pack-Idxing/
template<std::size_t Idx, typename... Pack>
using TypeAt = typename std::tuple_element<Idx, std::tuple<Pack...>>::type;

template<typename T, typename... Pack>
struct Index;
    
template<typename T, typename... Pack>
struct Index<T, T, Pack...> : std::integral_constant<std::size_t, 0> {};
    
template<typename T, typename U, typename... Pack>
struct Index<T, U, Pack...> : std::integral_constant<std::size_t, 1 + Index<T, Pack...>::value> {};
    
template<typename T, typename... Pack>
constexpr auto Index_v = Index<T, Pack...>::value;

static_assert(Index_v<int, int, int> == 0);
static_assert(Index_v<int, double, int> == 1);
static_assert(Index_v<int, double, float, char*, void*, int> == 4);

template<typename... Pack>
inline constexpr auto MaxElementSize() {
    return std::max({sizeof(Pack)...});
}

template<typename... Pack>
inline constexpr auto MaxAlignment() {
    return std::max({alignof(Pack)...});
}
    
static_assert(MaxElementSize<int, int, int>() == sizeof(int));
static_assert(MaxElementSize<int, double, int>() == sizeof(double));
static_assert(MaxAlignment<int, short, void*>() == alignof(void*));
    
template <typename T, typename... Pack>
static constexpr bool IsOneOf() {
    return std::disjunction_v<std::is_same<T, Pack>...>;
}
    
template <typename Element, typename... Ts>
using IsInPack_t = std::enable_if_t<IsOneOf<Element, Ts...>()>;
    
template <typename Element, typename... Ts>
constexpr bool IsInPack_v = IsOneOf<Element, Ts...>();
    
template <typename... Ts>
static constexpr bool IsAllowedInVariant() {
    return !(std::disjunction_v<std::is_reference<Ts>..., std::is_void<Ts>..., std::is_array<Ts>...>);
}
static_assert(!IsAllowedInVariant<int[3]>());
static_assert(!IsAllowedInVariant<int&>());
static_assert(!IsAllowedInVariant<void>());

template <typename Element, typename T, typename... Ts>
static constexpr size_t IndexForType() {
    if constexpr (IsInPack_v<Element, T, Ts...>) {
        return Index_v<Element, T, Ts...>;
    } else if constexpr (std::is_constructible_v<T, Element>) {
        return 0;
    } else {
        return 1 + IndexForType<Element, Ts...>();
    }
}
static_assert(0 == IndexForType<int, int, float, double>());
static_assert(1 == IndexForType<float, int, float, double>());
static_assert(2 == IndexForType<const char*, int, float, std::string>());
static_assert(1 == IndexForType<const char*, int, const char*, std::string>());
    
template<size_t Index, typename Variant>
decltype(auto) get(Variant&& v) {
    return v.template getAt<Index>();
}
    
template <typename Visitor, typename Variant>
auto visit(Visitor&& visitor, Variant&& v) -> decltype(visitor(get<0>(std::forward<Variant>(v))));

template <typename... Ts>
class Variant {
public:
    using IdxType = std::size_t;
    static constexpr auto Count = sizeof...(Ts);
    
    template <typename Element, typename = IsInPack_t<std::decay_t<Element>, Ts...>>
    constexpr Variant(Element&& element) {
        init<Index_v<std::decay_t<Element>, Ts...>>(std::forward<Element>(element));
    }
    
    template <typename Element,
              typename = std::enable_if_t<!IsInPack_v<std::decay_t<Element>, Ts...>>,
              typename = std::enable_if_t<std::disjunction_v<std::is_constructible<Ts, std::decay_t<Element>>...>>,
              IdxType Idx = IndexForType<Element, Ts...>()
             >
    constexpr Variant(Element&& element) noexcept(std::is_nothrow_constructible_v<TypeAt<Idx, Ts...>, Element>)
        : Variant(std::in_place_index<Idx>, std::forward<Element>(element)) {}
    
    template <IdxType Idx, typename... Args>
    constexpr Variant(std::in_place_index_t<Idx>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<TypeAt<Idx, Ts...>, Args...>) {
        init<Idx>(std::forward<Args>(args)...);
    }
    
    template <IdxType Idx, typename U, typename... Args>
    constexpr Variant(std::in_place_index_t<Idx>, std::initializer_list<U> il, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<TypeAt<Idx, Ts...>, std::initializer_list<U>, Args...>) {
        init<Idx>(std::move(il), std::forward<Args>(args)...);
    }
    
    IdxType getIndex() const noexcept {
        return typeIdx_;
    }
    
    constexpr Variant() noexcept(std::is_nothrow_default_constructible_v<TypeAt<0, Ts...>>) {
        init<0>();
    }
    
    constexpr Variant(const Variant& other) noexcept(NTCC) {
        visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            init<Index_v<T, Ts...>>(v);
        }, other);
        typeIdx_ = other.typeIdx_;
    }
    
    constexpr Variant(Variant&& other) noexcept(NTMC) {
        visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            init<Index_v<T, Ts...>>(std::move(v));
        }, other);
        typeIdx_ = other.typeIdx_;
    }
    
    // TODO: The use of noexcept here is a hammer, we can do it per-type
    template <typename T, typename = IsInPack_t<std::decay_t<T>, Ts...>>
    constexpr Variant& operator=(T&& val) noexcept(NTA) {
        destroy();
        init<Index_v<T, Ts...>>(std::forward<T>(val));
        return *this;
    }
    
    constexpr Variant& operator=(const Variant& other) noexcept(NTCA) {
        Variant tmp{other};
        std::swap(*this, tmp);
        return *this;
    }
    
    constexpr Variant& operator=(Variant&& other) noexcept(NTMA) {
        // Note that using swap here will cause infinite recursion since
        // swap internally uses move assignment
        destroy();
        new (this) Variant{std::move(other)};
        return *this;
    }
    
    template <std::size_t Idx, typename ReturnType = TypeAt<Idx, Ts...>>
    ReturnType& getAt() noexcept {
        static_assert(Idx < Count);
        return reinterpret_cast<ReturnType&>(storage_);
    }
    
    template <std::size_t Idx, typename ReturnType = TypeAt<Idx, Ts...>>
    constexpr const ReturnType& getAt() const noexcept {
        return reinterpret_cast<const ReturnType&>(storage_);
    }
    
    // Calling get() with the wrong type is UB, the std equivalent throws and exception
    // but we avoid that penalty and leave it up to the caller
    template <typename Element>
    constexpr Element& get() noexcept {
        static_assert(IsInPack_v<Element, Ts...>);
        return reinterpret_cast<Element&>(storage_);
    }
    
    template <typename Element>
    constexpr const Element& get() const noexcept {
        return reinterpret_cast<const Element&>(storage_);
    }
    
    template <typename Element, typename = IsInPack_t<Element, Ts...>>
    constexpr Element* getIf() noexcept {
        if (Index_v<Element, Ts...> == typeIdx_) {
            return reinterpret_cast<Element*>(&storage_);
        }
        return nullptr;
    }
    
    template <typename Element, typename = IsInPack_t<Element, Ts...>>
    constexpr const Element* getIf() const noexcept {
        if (Index_v<Element, Ts...> == typeIdx_) {
            return reinterpret_cast<const Element*>(&storage_);
        }
        return nullptr;
    }
    
    ~Variant() noexcept(NTD) {
        static_assert(IsAllowedInVariant<Ts...>(), "Cannot construct variant for this type");
        destroy();
    }
    
private:
    static constexpr auto NTD = std::conjunction_v<std::is_nothrow_destructible<Ts>...>;
    static constexpr auto NTMC = std::conjunction_v<std::is_nothrow_move_constructible<Ts>...>;
    static constexpr auto NTCC = std::conjunction_v<std::is_nothrow_copy_constructible<Ts>...>;
    static constexpr auto NTCA = std::conjunction_v<std::is_nothrow_copy_assignable<Ts>...>;
    static constexpr auto NTMA = std::conjunction_v<std::is_nothrow_move_assignable<Ts>...>;
    static constexpr auto NTA = NTCA && NTMA;
    
    template <IdxType Idx, typename... Args>
    void init(Args&&... args) {
        using T = TypeAt<Idx, Ts...>;
        typeIdx_ = Idx;
        new (&storage_) T(std::forward<Args>(args)...);
    }
    
    void destroy() noexcept(NTD) {
        // The alternative to this approach is to store a deleter function pointer as a member
        // variable. The drawback is that we pay for memory for each variant instantiaion at runtime.
        // Drawback with current approach is larger binary size due to this static array of function
        // pointers, but smaller footprint.
        visit([](auto& val) noexcept(std::is_nothrow_destructible_v<std::decay_t<decltype(val)>>) {
            using D = std::decay_t<decltype(val)>;
            if constexpr (!std::is_trivially_destructible_v<D>) {
                val.~D();
            }
        }, *this);
    }
    
    using Storage = std::aligned_storage_t<MaxElementSize<Ts...>(), MaxAlignment<Ts...>()>;
    Storage storage_;
    IdxType typeIdx_ = Count;
};

// TODO: noexcepts for visit
template<typename Visitor, typename Variant, bool UseLookupVisitor>
struct VisitHelper {
    static decltype(auto) run(Visitor&& visitor, Variant&& v) {
        if constexpr (UseLookupVisitor) {
            using IdxSeq = std::make_index_sequence<std::decay_t<Variant>::Count>;
            return run(std::forward<Visitor>(visitor), std::forward<Variant>(v), IdxSeq{});
        } else {
            return run<0>(std::forward<Visitor>(visitor), std::forward<Variant>(v));
        }
    }
    
    // Here, we statically store a lookup table of function pointers to call for visits. In cases
    // where the index is not known compile time, this method is more efficient since we can
    // directly lookup to make the call (although there is a penalty of accessing cold memory)
    // Also, we introduce an extra function in-direction.
    template <std::size_t... Idx>
    static decltype(auto) run(Visitor&& visitor, Variant&& v, std::index_sequence<Idx ...>) {
        using RetType = decltype(visitor(get<0>(std::forward<Variant>(v))));
        using VisitFn = RetType (*)(Visitor&&, Variant&&);
        static const std::array<VisitFn, sizeof...(Idx)> lookup = {
            [](Visitor&& visitor, Variant&& v) -> RetType {
                return visitor(std::forward<Variant>(v).template getAt<Idx>());
            }...,
        };
        return lookup[v.getIndex()](std::forward<Visitor>(visitor), std::forward<Variant>(v));
    }
    
    // This will be somewhat of a binary bloat for variants with a large pack, but where the
    // compiler knows index at compile time, it can easily collapse all the function calls
    // into the final direct call
    template <size_t Index>
    static decltype(auto) run(Visitor&& visitor, Variant&& v) {
        if constexpr (std::decay_t<Variant>::Count == Index) {
            // If variant is empty, return default initialized type at index 0
            return visitor(std::forward<Variant>(v).template getAt<0>());
        } else {
            if (v.getIndex() == Index) {
                return visitor(std::forward<Variant>(v).template getAt<Index>());
            } else {
                return run<Index + 1>(std::forward<Visitor>(visitor), std::forward<Variant>(v));
            }
        }
    }
};

template <typename Visitor, typename Variant>
auto visit(Visitor&& visitor, Variant&& v) -> decltype(visitor(get<0>(std::forward<Variant>(v)))) {
    static constexpr auto UseLookupVisitor = true;
    return VisitHelper<Visitor, Variant, UseLookupVisitor>::run(std::forward<Visitor>(visitor), std::forward<Variant>(v));
}

template <typename... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;
}
