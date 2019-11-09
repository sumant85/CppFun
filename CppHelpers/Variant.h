//
//  Variant.h
//  CppHelpers
//
//  Created by Sumant Hanumante on 9/21/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

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
        init<Element>(std::forward<Element>(element));
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
        using T = TypeAt<Idx, Ts...>;
        init<T>(std::forward<Args>(args)...);
    }
    
    // TODO: Simplify these noexcepts :|
    template <IdxType Idx, typename U, typename... Args>
    constexpr Variant(std::in_place_index_t<Idx>, std::initializer_list<U> il, Args&&... args)
    noexcept(std::is_nothrow_constructible_v<TypeAt<Idx, Ts...>, std::initializer_list<U>, Args...>) {
        using T = TypeAt<Idx, Ts...>;
        init<T>(std::move(il), std::forward<Args>(args)...);
    }
    
    IdxType getIndex() const noexcept {
        return typeIdx_;
    }
    
    Variant() noexcept(std::is_nothrow_default_constructible_v<TypeAt<0, Ts...>>) {
        using T = TypeAt<0, Ts...>;
        init<T>();
    }
    
    Variant(const Variant& other) noexcept(NTCC) {
        visit([&](const auto& v) {
            init(v);
        }, other);
    }
    
    Variant(Variant&& other) noexcept(NTMC) {
        visit([&](auto&& v) {
            init(std::move(v));
        }, other);
    }
    
    // TODO: The use of noexcept here is a hammer, we can do it per-type
    template <typename T, typename = IsInPack_t<std::decay_t<T>, Ts...>>
    Variant& operator=(T&& val) noexcept(NTA) {
        destroy();
        init<T>(std::forward<T>(val));
        return *this;
    }
    
    Variant& operator=(const Variant& other) noexcept(NTCA) {
        Variant tmp{other};
        std::swap(*this, tmp);
        return *this;
    }
    
    Variant& operator=(Variant&& other) noexcept(NTMA) {
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
    const ReturnType& getAt() const noexcept {
        return reinterpret_cast<const ReturnType&>(storage_);
    }
    
    // Calling get() with the wrong type is UB, the std equivalent throws and exception
    // but we avoid that penalty and leave it up to the caller
    template <typename Element>
    Element& get() noexcept {
        static_assert(IsInPack_v<Element, Ts...>);
        return reinterpret_cast<Element&>(storage_);
    }
    
    template <typename Element>
    const Element& get() const noexcept {
        return reinterpret_cast<const Element&>(storage_);
    }
    
    template <typename Element, typename = IsInPack_t<Element, Ts...>>
    Element* getIf() noexcept {
        if (Index_v<Element, Ts...> == typeIdx_) {
            return reinterpret_cast<Element*>(&storage_);
        }
        return nullptr;
    }
    
    template <typename Element, typename = IsInPack_t<Element, Ts...>>
    const Element* getIf() const noexcept {
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
    
    using Deleter = void (*)(void*) noexcept(NTD);
    template <typename T>
    static Deleter getDeleter() {
        if (std::is_trivially_destructible_v<T>) {
            return [](void *) noexcept(NTD) {};
        }
        return [](void * ptr) noexcept(NTD) { static_cast<T*>(ptr)->~T(); };
    }
    
    template <typename T>
    void init(T&& element) {
        using D = std::decay_t<T>;
        typeIdx_ = Index_v<D, Ts...>;
        new (&storage_) D(std::forward<T>(element));
        deleter_ = getDeleter<D>();
    }
    
    template <typename T, typename... Args>
    void init(Args&&... args) {
        using D = std::decay_t<T>;
        typeIdx_ = Index_v<D, Ts...>;
        new (&storage_) D(std::forward<Args>(args)...);
        deleter_ = getDeleter<D>();
    }
    
    void destroy() noexcept(NTD) {
        assert(deleter_);
        deleter_(&storage_);
    }
    
    std::aligned_storage_t<MaxElementSize<Ts...>(), MaxAlignment<Ts...>()> storage_;
    IdxType typeIdx_ = Count;
    
    // We pay for storing an extra deleter member variable but save runtime cost
    // to peform the actual lookup on the type on destruction. The additional
    // storage is sizeof(void*)
    Deleter deleter_ = nullptr;
};

// This will be somewhat of a binary bloat for variants with a large pack
// An alternative is to statically construct an array of function pointers for each
// visit instantiation and just directly index into that array
template<typename Visitor, typename Variant, std::size_t Index>
struct VisitHelper {
    static decltype(auto) run(Visitor&& visitor, Variant&& v) {
        if constexpr (std::decay_t<Variant>::Count == Index) {
            // If variant is empty, return default initialized type at index 0
            return visitor(std::forward<Variant>(v).template getAt<0>());
        } else {
            if (v.getIndex() == Index) {
                return visitor(std::forward<Variant>(v).template getAt<Index>());
            } else {
                return VisitHelper<Visitor, Variant, Index + 1>::run(std::forward<Visitor>(visitor), std::forward<Variant>(v));
            }
        }
    }
};
    
//template<typename R, typename Visitor, typename Var, typename... Ts>
//struct VisitHelperFPtrs_table {
//    static R run(Visitor&& visitor, Var&& v, const Variant<Ts...> *) {
//        constexpr static R (*visitTable[std::decay_t<Var>::Count])(Visitor&&, Var&&) {
//            [](Visitor&& vi, Var&& var) { return vi(var.template get<Ts>()); }...
//        };
//
//        return visitTable[v.getIndex()](std::forward<Visitor>(visitor), std::forward<Var>(v));
//    }
//};
//
//template <typename Visitor, typename Variant, std::size_t>
//struct VisitHelperFPtrs {
//    static decltype(auto) run(Visitor&& visitor, Variant&& v) {
//        using R = decltype(visitor(v.template getAt<0>()));
//        return VisitHelperFPtrs_table<R, Visitor, Variant>::run(
//                std::forward<Visitor>(visitor), std::forward<Variant>(v), &v);
//    }
//};

template <typename Visitor, typename Variant>
auto visit(Visitor&& visitor, Variant&& v) -> decltype(visitor(get<0>(std::forward<Variant>(v)))) {
    return VisitHelper<Visitor, Variant, 0>::run(std::forward<Visitor>(visitor), std::forward<Variant>(v));
}
    
template <typename... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;
}
