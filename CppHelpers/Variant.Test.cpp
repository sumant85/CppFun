//
//  Variant.Test.cpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 10/12/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#include <catch2/catch.hpp>

#include "Variant.h"
#include "NonCopyable.h"
#include "NonMovable.h"

#include <array>
#include <memory>

namespace {
struct MoveThrows {
    MoveThrows() = default;
    MoveThrows(const MoveThrows&) = default;
    MoveThrows(MoveThrows&&) noexcept(false);
};

struct CopyThrows {
    CopyThrows() = default;
    CopyThrows(const CopyThrows&) noexcept(false);
    CopyThrows(CopyThrows&&) = delete;
};

struct DestrThrows {
    ~DestrThrows() noexcept(false) { throw 1; }
};

struct NonMovableNonCopyable : public sh::NonMovable, public sh::NonCopyable {};

constexpr auto str = "hello world";
}

TEST_CASE("[Variant] noexcept correctness") {
    using V = sh::Variant<int, char, float, std::array<int, 10>>;
    static_assert(std::is_nothrow_move_constructible_v<V>);
    static_assert(std::is_nothrow_copy_constructible_v<V>);
    static_assert(std::is_nothrow_destructible_v<V>);
    static_assert(std::is_trivially_destructible_v<V>);
    static_assert(std::is_trivially_destructible_v<std::array<V, 10>>);

    using V1 = sh::Variant<int, char, MoveThrows>;
    static_assert(!std::is_nothrow_move_constructible_v<V1>);
    static_assert(std::is_nothrow_destructible_v<V1>);
    static_assert(std::is_trivially_destructible_v<V1>);

    using V2 = sh::Variant<int, char, CopyThrows>;
    static_assert(!std::is_nothrow_copy_constructible_v<V2>);
    static_assert(std::is_trivially_destructible_v<V2>);

    static_assert(noexcept(std::swap(std::declval<V&>(), std::declval<V&>())));
    static_assert(!noexcept(std::swap(std::declval<V1&>(), std::declval<V1&>())));
    
    using V3 = sh::Variant<int, DestrThrows>;
    static_assert(!std::is_nothrow_destructible_v<V3>);
    static_assert(!std::is_trivially_destructible_v<V3>);
    
    static_assert(2 == sh::detail::IndexForType<const char*, int, float, std::string>());
    static_assert(1 == sh::detail::IndexForType<const char*, int, const char*, std::string>());
    
    using V4 = sh::Variant<std::shared_ptr<bool>, std::vector<std::string>>;
    V4 v;
    static_assert(std::is_lvalue_reference_v<decltype(sh::get<0>(v))>);
    
    {
        using V = sh::Variant<int, float>;
        sh::Overloaded overload {
            [](int i) noexcept {},
            [](float f) noexcept(false) { throw 1; },
        };
        static_assert(!noexcept(sh::visit(overload, V{})));
        
        sh::Overloaded overloadNoExcept {
            [](int i) noexcept {},
            [](float f) noexcept {}
        };
        static_assert(noexcept(sh::visit(overloadNoExcept, V{})));
    }
}

TEST_CASE("[Variant] Constructing variants") {
    SECTION("Default construction") {
        sh::Variant<int, float, double, bool> var;
        REQUIRE(var.getIndex() == 0);
        
        sh::Variant<NonMovableNonCopyable, std::string, bool> var1;
        REQUIRE(var1.getIndex() == 0);
    }

    SECTION("Type Deduction") {
        sh::Variant<int, float, double, bool> var1{1.f};
        REQUIRE(var1.getIndex() == 1);
        
        sh::Variant<int, float, double, bool> var2{2.0};
        REQUIRE(var2.getIndex() == 2);
        
        struct TakesInt {
            TakesInt(int a) : val(a) {}
            int val;
        };
        SECTION("Choose direct type over constructible types") {
            sh::Variant<TakesInt, int> var{10};
            REQUIRE(var.getIndex() == 1);
        }
        SECTION("Choose first constructible type when no direct match") {
            sh::Variant<TakesInt, int> var1{10.0};
            REQUIRE(var1.getIndex() == 0);
        }
    }

    SECTION("Using type index") {
        constexpr auto str = "hello world";
        using V = sh::Variant<std::vector<int>, std::string, float>;
        V var1(std::in_place_index<1>, str);
        REQUIRE(var1.get<std::string>() == str);

        V var2(std::in_place_index<0>, {1, 2, 3, 4});
        auto& res = *var2.getIf<std::vector<int>>();
        REQUIRE(res.size() == 4);
        auto idx = 1;
        std::for_each(res.begin(), res.end(), [&](int val) { REQUIRE(val == idx++); });
        
        V var3(std::in_place_index<2>, 10); // int -> float conversion
        REQUIRE(var3.getIndex() == 2);
        REQUIRE(var3.getAt<2>() == 10.f);
        
        sh::Variant<std::string, NonMovableNonCopyable> var4(std::in_place_index<1>);
        REQUIRE(var4.getIndex() == 1);
    }
    
    SECTION("Copy and move construction") {
        using V = sh::Variant<bool, std::shared_ptr<bool>, int, std::shared_ptr<bool>>;
        auto ptr = std::make_shared<bool>(true);
        V var(std::in_place_index<3>, ptr);
        REQUIRE(ptr.use_count() == 2);
        
        auto copy = var;
        REQUIRE(ptr.use_count() == 3);
        SECTION("Variants preserve index on copy construction") {
            REQUIRE(copy.getIndex() == 3);
        }
        
        auto move = std::move(var);
        REQUIRE(ptr.use_count() == 3); // one in copy, one in move, one in ptr
        SECTION("Variants preserve index on move construction") {
            REQUIRE(move.getIndex() == 3);
        }
        
        SECTION("Variants choose first possible index on assignment") {
            V var = std::make_shared<bool>(true);
            REQUIRE(var.getIndex() == 1);
        }
    }
}

TEST_CASE("[Variant] Destructing variants") {
    SECTION("Ensure non-trivial object is destroyed") {
        auto ptr = std::make_shared<bool>(true);
        std::weak_ptr<bool> wPtr = ptr;
        {
            sh::Variant<bool, std::shared_ptr<bool>, int> var(std::move(ptr));
            REQUIRE(wPtr.use_count() == 1);
        }
        REQUIRE(wPtr.expired());
    }
    
    SECTION("Throwing destructors") {
        SECTION("Doesn't throw") {
            sh::Variant<int, DestrThrows> v(10);
        }
        
        SECTION("Doesn't terminate") {
            bool threw = false;
            try {
                sh::Variant<int, DestrThrows> v{DestrThrows()};
            } catch(...) {
                threw = true;
            }
            REQUIRE(threw);
        }
    }
}

TEST_CASE("[Variant] Assigning to variants") {
    SECTION("Copy assign") {
        sh::Variant<std::shared_ptr<int>> var1(std::make_shared<int>(1));
        sh::Variant<std::shared_ptr<int>> var2;

        var2 = var1;
        REQUIRE(var1.get<std::shared_ptr<int>>().use_count() == 2);
        REQUIRE(var2.get<std::shared_ptr<int>>().use_count() == 2);
    }

    SECTION("Move assign") {
        {
            sh::Variant<std::shared_ptr<int>> var1(std::make_shared<int>(1));
            sh::Variant<std::shared_ptr<int>> var2;
            var2 = std::move(var1);
            SECTION("Internal object is destroyed on move") {
                REQUIRE(var1.get<std::shared_ptr<int>>().use_count() == 0);
            }
            REQUIRE(var2.get<std::shared_ptr<int>>().use_count() == 1);
            REQUIRE(*var2.get<std::shared_ptr<int>>() == 1);
        }
    }
    
    SECTION("Variants preserve index on move and assign") {
        using V = sh::Variant<bool, int, std::string, int>;
        V var(std::in_place_index<3>, 10);
        REQUIRE(var.getIndex() == 3);
        
        V copy;
        copy = var;
        REQUIRE(copy.getIndex() == 3);
        REQUIRE(copy.getAt<3>() == 10);
        
        V move;
        move = std::move(var);
        REQUIRE(move.getIndex() == 3);
        REQUIRE(move.getAt<3>() == 10);
    }

    SECTION("Assign different type") {
        sh::Variant<int, std::string, double> var(10);
        REQUIRE(var.get<int>() == 10);
        
        var = str;
        REQUIRE(var.get<std::string>() == str);
        REQUIRE(var.getIndex() == 1);
    }
}

TEST_CASE("[Variant] Visiting ") {
    using V = sh::Variant<int, std::string, double>;
    SECTION("Check visit with lambda") {
        V var(str);
        auto visited = false;
        sh::visit([&] (const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::string>) {
                visited = true;
                REQUIRE(s == str);
            }
        }, var);
        REQUIRE(visited);
    }

    SECTION("Check visit with overload") {
        V var(str);
        auto visited = false;
        sh::visit(sh::Overloaded {
            [&](std::string& s) {
                visited = true;
                REQUIRE(s == str);
                s = "hello";
            },
            [](const int& i) {},
            [](const double& d) {},
        }, var);
        REQUIRE(visited);
        REQUIRE(var.get<std::string>() == "hello");
    }
    
    SECTION("Check visit of const variant") {
        const V var(str);
        auto visited = false;
        sh::visit([&] (auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, std::string>) {
                visited = true;
            }
            static_assert(std::is_const_v<std::remove_reference_t<decltype(s)>>, "");
        }, var);
        REQUIRE(visited);
    }
    
    SECTION("Check visit returns") {
        std::vector<V> vec;
        vec.push_back(1);
        vec.push_back("a");
        vec.push_back(2.0);
        
        auto ret = sh::visit([](auto&& arg) -> V {return arg + arg; }, vec[0]);
        REQUIRE(ret.getAt<0>() == 2);
        
        ret = sh::visit([](auto&& arg) -> V {return arg + arg; }, vec[1]);
        REQUIRE(ret.getAt<1>() == "aa");
        
        ret = sh::visit([](auto&& arg) -> V {return arg + arg; }, vec[2]);
        REQUIRE(ret.getAt<2>() == 4.0);
    }

    SECTION("Check visit respects r-values") {
        sh::Variant<int, std::shared_ptr<int>> var(std::make_shared<int>(10));

        auto ptr = std::make_shared<int>(1);
        sh::visit([&ptr] (auto&& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<int>>) {
                val = std::move(ptr);
            }
        }, var);

        REQUIRE(ptr == nullptr);
        REQUIRE(*var.get<std::shared_ptr<int>>() == 1);
    }
    
    SECTION("Visit with exceptions") {
        sh::Variant<int, float> v{1.f};
        bool threw = false;
        try {
            sh::visit([](auto& val) noexcept(false) {
               throw 1;
           }, v);
        } catch (...) { threw = true; }
        REQUIRE(threw);

        sh::Overloaded overload {
            [](int i) noexcept {},
            [](float f) noexcept(false) { throw 1; }
        };

        threw = false;
        try {
           sh::visit(overload, v);
        } catch (...) { threw = true; }
        REQUIRE(threw);
        
        v = 2;
        threw = false;
        try {
           sh::visit(overload, v);
        } catch (...) { threw = true; }
        REQUIRE(!threw);
    }
}

TEST_CASE("[Variant] Using get ") {
    using V = sh::Variant<std::shared_ptr<int>, std::string, double>;
    V var(2.0);
    REQUIRE(sh::get<2>(var) == 2.0);
    
    REQUIRE(sh::get<2>(V{1.0}) == 1.0);
    
    V var1(str);
    REQUIRE(sh::get<1>(var1) == str);
    
    V var2(std::make_shared<int>(1));
    REQUIRE(*sh::get<0>(var2) == 1);
    auto ptr = std::move(sh::get<0>(var2));
    REQUIRE(*ptr == 1);
    REQUIRE(sh::get<0>(var2) == nullptr);
}

// Section of stuff I still need to figure out how to fix
TEST_CASE("[Variant] Gotchas ") {
    SECTION("Explicit conversions") {
        using V = sh::Variant<bool, std::string>;
        // It's possible to special case for char and string types, but should I?...
        V var("hello");
        REQUIRE(var.getIndex() == 0); // Should ideally be 1, but we end up picking bool
    }
    
    SECTION("Give preference to non-narrowing") {
        struct TakesDouble {
            TakesDouble(double a) : val(a) {}
            double val;
        };
        sh::Variant<int, TakesDouble> var2{10.0};
        REQUIRE(var2.getIndex() == 0); // Should ideally be 1
    }
}
