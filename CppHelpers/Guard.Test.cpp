//
//  GuardTests.cpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 8/30/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#include <catch2/catch.hpp>

#include "Guard.h"

#include <typeinfo>
#include <type_traits>

TEST_CASE("Guard created on stack", "[StackGuard]") {
    SECTION("non-throwing guard executes on scope exit") {
        int val = 1;
        {
            auto guard = sh::StackGuard([&]() {
                val = 2;
            });
            REQUIRE(val == 1);
        }
        REQUIRE(val == 2);
        
        {
            std::function<void(void)> fun = [&]() {
                val = 3;
            };
            auto guard = sh::StackGuard(std::move(fun));
        }
        REQUIRE(val == 3);
    }
    
    SECTION("throwing guard executes, doesn't terminate") {
        int val = 1;
        try {
            auto guard = sh::StackGuard([&]() {
                val = 2;
                throw std::runtime_error("");
            });
            REQUIRE(val == 1);
        } catch (std::exception&) {}
        REQUIRE(val == 2);
    }
    
    SECTION("throwing guard executes, and doesn't leak") {
        auto ptr = std::make_shared<int>(10);
        try {
            auto guard = sh::StackGuard([ptr = ptr]() {
                REQUIRE(ptr.use_count() == 2);
                throw std::runtime_error("");
            });
        } catch (std::exception&) {}
        REQUIRE(ptr.use_count() == 1);
    }
    
    SECTION("dismissable guard") {
        int val = 1;
        {
            auto guard = sh::StackGuard([&]() {
                val = 2;
            });
            REQUIRE(val == 1);
            guard.dismiss();
        }
        REQUIRE(val == 1);
    }
}
    
TEST_CASE("Guard created on heap", "[GuardBase]") {
    struct Holder {
        sh::GuardKey guard;
    };
    
    struct NonTrivialFunctor {
        void operator()() noexcept(true) {
            if (member) {
                *member = !(*member);
            }
        }
        
        std::shared_ptr<bool> member;
    };
    
    struct TrivialFunctor {
        void operator()() noexcept(true) {
            a++;
            b = !b;
        }
        
        int a;
        bool b;
    };
    
    SECTION("Guard key executes") {
        SECTION("With lambda") {
            int valA = 1;
            int valB = 2;
            Holder h;
            h.guard = sh::makeGuard([&]() noexcept(true) {
                valA = 2;
                valB = 3;
            });
            
            REQUIRE(valA == 1);
            REQUIRE(valB == 2);
            h.guard = nullptr;
            REQUIRE(valA == 2);
            REQUIRE(valB == 3);
        }
        
        SECTION("With functor") {
            NonTrivialFunctor functor;
            auto ptr = std::make_shared<bool>(true);
            functor.member = ptr;
            
            Holder h;
            h.guard = sh::makeGuard(std::move(functor));
            
            REQUIRE(*ptr == true);
            h.guard = nullptr;
            REQUIRE(*ptr == false);
        }
    }
    
    SECTION("Guard key can be dismissed") {
        SECTION("Non-trivial functor") {
            NonTrivialFunctor functor;
            auto ptr = std::make_shared<bool>(true);
            functor.member = ptr;
            
            Holder h;
            h.guard = sh::makeGuard(std::move(functor));

            h.guard->dismiss();
            REQUIRE(*ptr == true);
            h = {};
            REQUIRE(*ptr == true);
        }
        
        SECTION("Trivial functor") {
            TrivialFunctor functor;
            functor.a = 0;
            functor.b = false;
            
            Holder h;
            h.guard = sh::makeGuard(std::move(functor));
            
            h.guard->dismiss();
            h = {};
            REQUIRE(functor.a == 0);
            REQUIRE(functor.b == false);
        }
    }
    
    SECTION("Target is deallocated") {
        auto owner = std::make_shared<int>(10);
        std::weak_ptr<int> weakPtr = owner;
        auto target = [ptr = std::move(owner)]() noexcept(true) {};
        
        Holder h;
        h.guard = sh::makeGuard(std::move(target));
        REQUIRE(weakPtr.use_count() == 1);
        h.guard = nullptr;
        REQUIRE(weakPtr.use_count() == 0);
    }
}

