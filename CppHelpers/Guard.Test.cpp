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
    
    SECTION("Trivial guard") {
        int valA = 1;
        int valB = 2;
        Holder h;
        h.guard = sh::makeGuard([&]() noexcept(true) {
            valA = 2;
            valB = 3;
        });
        REQUIRE(valA == 1);
        REQUIRE(valB == 2);
        
        auto& ref = *h.guard;
        // Capturing 2 refs, so lambda should should be of 2 * size(void*)
        REQUIRE(typeid(ref) == typeid(sh::TrivialGuard<2 * sizeof(void*)>));
        
        h = {};
        REQUIRE(valA == 2);
        REQUIRE(valB == 3);
        
        TrivialFunctor functor;
        h.guard = sh::makeGuard(std::move(functor));
        ref = *h.guard;
        REQUIRE(typeid(ref) == typeid(sh::TrivialGuard<sh::SizeInBytes<TrivialFunctor>()>));
    }
    
    SECTION("Non-trivial guard") {
        NonTrivialFunctor functor;
        auto ptr = std::make_shared<bool>(true);
        functor.member = ptr;
        
        Holder h;
        h.guard = sh::makeGuard(std::move(functor));
        auto& ref = *h.guard;
        REQUIRE(typeid(ref) == typeid(sh::Guard<NonTrivialFunctor>));
        
        h = {};
        REQUIRE(*ptr == false);
    }
    
    SECTION("Non-trivial guard dismissed") {
        NonTrivialFunctor functor;
        auto ptr = std::make_shared<bool>(true);
        functor.member = ptr;
        
        Holder h;
        h.guard = sh::makeGuard(std::move(functor));
        auto& ref = *h.guard;
        REQUIRE(typeid(ref) == typeid(sh::Guard<NonTrivialFunctor>));
        
        h.guard->dismiss();
        h = {};
        REQUIRE(*ptr == true);
    }
    
    SECTION("Trivial guard dismissed") {
        TrivialFunctor functor;
        functor.a = 0;
        functor.b = false;
        
        Holder h;
        h.guard = sh::makeGuard(std::move(functor));
        auto& ref = *h.guard;
        REQUIRE(typeid(ref) == typeid(sh::TrivialGuard<sizeof(TrivialFunctor)>));
        
        h.guard->dismiss();
        h = {};
        REQUIRE(functor.a == 0);
        REQUIRE(functor.b == false);
    }
}

