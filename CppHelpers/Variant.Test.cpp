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

struct NonMovableNonCopyable : public sh::NonMovable, public sh::NonCopyable {};

constexpr auto str = "hello world";
}

TEST_CASE("noexcept correctness ", "[Variant]") {
    using V = sh::Variant<int, char, float>;
    static_assert(std::is_nothrow_move_constructible_v<V>, "");
    static_assert(std::is_nothrow_copy_constructible_v<V>, "");

    using V1 = sh::Variant<int, char, MoveThrows>;
    static_assert(!std::is_nothrow_move_constructible_v<V1>, "");

    using V2 = sh::Variant<int, char, CopyThrows>;
    static_assert(!std::is_nothrow_copy_constructible_v<V2>, "");

    static_assert(noexcept(std::swap(std::declval<V&>(), std::declval<V&>())), "");
    static_assert(!noexcept(std::swap(std::declval<V1&>(), std::declval<V1&>())), "");
}

TEST_CASE("Constructing variants ", "[Variant]") {
    SECTION("Default construction") {
        sh::Variant<int, float, double, bool> var;
        REQUIRE(var.getIndex() == 0);
    }

    SECTION("Type Deduction") {
        sh::Variant<int, float, double, bool> var1{1.f};
        REQUIRE(var1.getIndex() == 1);
        
        sh::Variant<int, float, double, bool> var2{2.0};
        REQUIRE(var2.getIndex() == 2);
    }

    SECTION("Using type index") {
        sh::Variant<int, float, double, bool> var(std::in_place_index<2>, 2.0);
        REQUIRE(var.getIndex() == 2);
        REQUIRE(*var.getIf<double>() == 2.0);
        REQUIRE(var.get<double>() == 2.0);
    }

    SECTION("Complex types using type index (including initializer list)") {
        constexpr auto str = "hello world";
        sh::Variant<std::vector<int>, std::string, bool> var1(std::in_place_index<1>, str);
        REQUIRE(var1.get<std::string>() == str);

        sh::Variant<std::vector<int>, std::string, bool> var2(std::in_place_index<0>, {1, 2, 3, 4});
        auto& res = *var2.getIf<std::vector<int>>();
        REQUIRE(res.size() == 4);
        auto idx = 1;
        std::for_each(res.begin(), res.end(), [&](int val) { REQUIRE(val == idx++); });
    }

    SECTION("Check peculiar types") {
        sh::Variant<NonMovableNonCopyable, std::string, bool> var;
        REQUIRE(var.getIndex() == 0);

        sh::Variant<std::string, NonMovableNonCopyable> var1(std::in_place_index<1>);
        REQUIRE(var1.getIndex() == 1);
    }
}

TEST_CASE("Assigning to variants ", "[Variant]") {
    SECTION("Copy assign") {
        sh::Variant<std::shared_ptr<int>> var1(std::make_shared<int>(1));
        auto var2 = var1;
        REQUIRE(var1.get<std::shared_ptr<int>>().use_count() == 2);
        REQUIRE(var2.get<std::shared_ptr<int>>().use_count() == 2);
    }

    SECTION("Move assign") {
        std::weak_ptr<int> wPtr;
        {
            sh::Variant<std::shared_ptr<int>> var1(std::make_shared<int>(1));
            auto var2 = std::move(var1);
            SECTION("Internal object is destroyed on move") {
                REQUIRE(var1.get<std::shared_ptr<int>>() == nullptr);
            }
            REQUIRE(var2.get<std::shared_ptr<int>>().use_count() == 1);
            REQUIRE(*var2.get<std::shared_ptr<int>>() == 1);
            wPtr = var2.get<std::shared_ptr<int>>();
        }

        SECTION("All objects destroyed") {
            REQUIRE(wPtr.use_count() == 0);
        }
    }

    SECTION("Assign different type") {
        sh::Variant<int, std::string, double> var(10);
        REQUIRE(var.get<int>() == 10);
        
        var = str;
        REQUIRE(var.get<std::string>() == str);
    }
}

TEST_CASE("Visiting variants ", "[Variant]") {
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
}

TEST_CASE("Using get ", "[Variant]") {
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

TEST_CASE("Gotchas ", "[Variant]") {
    // Section of stuff I still need to figure out how to fix
    using V = sh::Variant<bool, std::string>;
    V var("hello");
    REQUIRE(var.getIndex() == 0); // Should ideally be 1, but we end up picking bool
}
