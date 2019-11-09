//
//  main.cpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 8/11/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#include <iostream>
#include "Guard.h"
#include "Variant.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>


template <typename T>
constexpr auto type_name()
{
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

#if 0

Learnings about SFINAE:
- template deduction and auto deduction behave exactly the same. There are special-cased for universal references.
- never deduce reference (so it must be explicitly specified as ref/universal ref using &&)
- Never deduces a type to an rvalue
- Constness is preserved
- For universal reference:
- An lvalue gets deduced to lvalue reference
- An rvalue (both prvalue and xrvalue) gets deduced to lvalue (and not reference)

Example:
template<typename T>
T&& foo(T&& t) { ... } // t will eventually always be passed by refence (T would either be lvalue or lvalue reference)

template<typename T> // will never deduce reference in this case
T bar(T t) { ... }

int a;
int& b = a;
int&& c = std::move(a);

foo(a); // T === int&
foo(b); // T === int&
foo(std::move(a)); // T === int
foo(c); // T === int& (since c is an lvalue of type rvalue reference)
foo<int&&>(std::move(c)); // T === int&& since we've set it explicitly, reference collapsing still gives decltype(t) == int&&

bar(a); // T === int
bar(b); // T === int
bar(std::move(a)); // T === int
bar(c); // T === int

#endif

TEST_CASE( "Prototyping", "quick prototyping" ) {
    struct Test {
        Test() {
            std::cout << "[Test] Constructed" << std::endl;
        }
        
        Test(Test&& o) {
            std::cout << "[Test] Move Constructed " << std::endl;
            ptr = std::move(o.ptr);
        }
        
        Test(const Test& o) {
            std::cout << "[Test] Copy Constructed" << std::endl;
            ptr = o.ptr;
        }
        
        Test& operator=(const Test& o) {
            std::cout << "[Test] Copy Assign" << std::endl;
            ptr = o.ptr;
            return *this;
        }
        
        Test& operator=(Test&& o) {
            std::cout << "[Test] Move Assign" << std::endl;
            ptr = std::move(o.ptr);
            return *this;
        }
        
        ~Test() {
            std::cout << "[Test] Destroyed" << std::endl;
        }

        std::shared_ptr<bool> ptr = std::make_shared<bool>(true);
    };
    
    {
        Test t{};
        sh::Variant<int, bool, double, Test> v1(std::move(t));
        sh::visit([](const auto& v) {
            auto& t = reinterpret_cast<const Test&>(v);
            std::cout << "[Type] " << type_name<decltype(t)>() << " "
                << t.ptr.use_count() << " " << sizeof(Test) << std::endl;
        }, v1);
        
        auto v2 = v1;
        sh::visit([](auto&& v) { std::cout << "[Type] " << type_name<decltype(v)>() << std::endl; }, v2);
        auto v3 = std::move(v2);
        sh::visit([](auto&& v) { std::cout << "[Type] " << type_name<decltype(v)>() << std::endl; }, v3);
        v3 = v1;
        sh::visit([](auto&& v) {
            std::cout << "[Type] " << type_name<decltype(v)>() << std::endl;
        }, v3);
        
        sh::visit([](auto&& v) { std::cout << "[Type] " << type_name<decltype(v)>() << std::endl; }, v1);
        v1 = false;
        sh::visit([](auto&& v) { std::cout << "[Type] " << type_name<decltype(v)>() << std::endl; }, v1);
        
        sh::visit(sh::Overloaded {
            [](auto arg) { std::cout << arg << ' '; },
            [](double arg) { std::cout << std::fixed << arg << ' '; },
            [](bool arg) { std::cout << arg << ' '; },
            [](const Test& arg) { std::cout << "Test" << " " << arg.ptr.use_count() << ' '; },
        }, v3);
    }
    
    struct Foo {
        std::unique_ptr<sh::GuardBase> g1;
        std::unique_ptr<sh::GuardBase> g2;
    };
    Foo f;
    f.g1 = sh::makeGuard([&]() noexcept {
        if (f.g1) {
            std::cout << "g1\n";
        }
    });
    
    std::shared_ptr<int> sp = std::make_shared<int>(10);
    f.g2 = sh::makeGuard([&, sp]() noexcept {
        std::cout << "g2" << *sp << std::endl;
    });
    
    // insert code here...
    try {
        auto guard = sh::StackGuard([&]() {
            std::cout << "Hello, World!\n";
            if (f.g1) {
                throw std::runtime_error("Error");
            }
        });
    } catch (std::runtime_error& e) {
        std::cout << "Stack guard threw!\n";
    }
}
