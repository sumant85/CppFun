//
//  ArrayVector.Test.cpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 12/10/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#include <catch2/catch.hpp>

#include "ArrayVector.h"
#include "NonCopyable.h"
#include "NonMovable.h"

#include <memory>

namespace {
struct Counter {
    static inline auto cnt = 0;
    Counter() { cnt++; }
    Counter(Counter&& o) { cnt++; }
    Counter(const Counter& o) { cnt++; }
    ~Counter() { cnt--; }
};
}

TEST_CASE("[ArrayVector] construction") {
    SECTION("Default construction") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        Vec v0;
        REQUIRE(v0.empty());
        REQUIRE(v0.size() == 0);
        
        auto ptr = std::make_shared<bool>();
        Vec v1(9, ptr);
        REQUIRE(ptr.use_count() == 10);
        REQUIRE(v1.size() == 9);
        
        Vec v2(4);
        std::for_each(v2.begin(), v2.end(), [](const auto& val) { REQUIRE(val == nullptr); });
        
        struct NoInit {
            NoInit() { throw 1; }
            ~NoInit() noexcept(false) { throw 1; }
        };
        sh::ArrayVector<NoInit, 2> vec;
        REQUIRE(true);
    }
    
    SECTION("Copy construction") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        Vec v0(4, ptr);
        Vec v1 = v0;
        REQUIRE(ptr.use_count() == 9);
        
        using VecT = sh::ArrayVector<int, 10>;
        VecT vt0(4, 10);
        VecT vt1 = vt0;
        for (int i = 0 ; i < vt0.size(); ++i) {
            REQUIRE(vt0[i] == vt1[i]);
        }
    }
    
    SECTION("Initializer list") {
        using Vec = sh::ArrayVector<int, 10>;
        Vec v0{1, 2, 3, 4};
        REQUIRE(v0.size() == 4);
    }
    
    SECTION("Move construction") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        Vec v0(4, ptr);
        Vec v1 = std::move(v0);
        REQUIRE(ptr.use_count() == 5);
        REQUIRE(v0.size() == 0);
    }
    
    SECTION("Iterator construction") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        Vec v(4, ptr);
        Vec v1(v.begin(), v.end());
        REQUIRE(ptr.use_count() == 9);
    }
    
    SECTION("2d array Vector") {
        using Vec = sh::ArrayVector<sh::ArrayVector<bool, 5>, 6>;
        Vec v(6, sh::ArrayVector<bool, 5>(5, true));
        REQUIRE(v.capacity() == 6);
        REQUIRE(v.front().capacity() == 5);
        
        for (int i = 0; i < v.size(); ++i) {
            for (int j = 0; j < v[0].size(); ++j) {
                REQUIRE(v[i][j]);
            }
        }
    }
    
    SECTION("Default-deleted construction") {
        struct DefaultDelete {
            DefaultDelete() = delete;
            DefaultDelete(int i) : i_(i) {}
            int i_;
        };
        using Vec = sh::ArrayVector<DefaultDelete, 10>;
        Vec v;
        v.push_back(DefaultDelete{1});
    }
    
    SECTION("Move-only types") {
        using Vec = sh::ArrayVector<std::unique_ptr<bool>, 10>;
        Vec v(2);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == nullptr);
        REQUIRE(v[0] == v[1]);
        v.push_back(std::make_unique<bool>(true));
        
        Vec v1 = std::move(v);
        REQUIRE(v1.size() == 3);
        REQUIRE(v1[2] != nullptr);
        REQUIRE(*v1[2] == true);
    }
}

TEST_CASE("[ArrayVector] destruction") {
    SECTION("Elements destroyed") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        {
            Vec v(5, ptr);
            REQUIRE(ptr.use_count() == 6);
            
            auto ptr1 = ptr;
            v.push_back(std::move(ptr1));
            REQUIRE(ptr.use_count() == 7);
        }
        REQUIRE(ptr.unique());
        
        Counter::cnt = 0;
        {
            using Vec = sh::ArrayVector<Counter, 10>;
            Vec v(1);
            REQUIRE(Counter::cnt == 1);
            v.push_back(Counter());
            REQUIRE(Counter::cnt == 2);
            
            auto v1 = std::move(v);
            auto v2 = std::move(v1);
            auto v3 = v2;
            for (int i = 0; i < 5; ++i) {
                v2.push_back({});
                v3.push_back({});
            }
        }
        REQUIRE(Counter::cnt == 0);
    }
}

TEST_CASE("[ArrayVector] affordances") {
    SECTION("push_back") {
        {
            using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
            Vec v;
            REQUIRE(v.size() == 0);
            for (int i = 1; i < 10; ++i) {
                v.push_back(std::make_shared<bool>(true));
                REQUIRE(v.size() == i);
            }
            std::for_each(v.begin(), v.end(), [](const auto& val) {
                REQUIRE(val.unique());
                REQUIRE(*val == true);
            });
        }
        {
            using Vec = sh::ArrayVector<std::unique_ptr<bool>, 10>;
            Vec v;
            REQUIRE(v.size() == 0);
            for (int i = 1; i < 10; ++i) {
                v.push_back(std::make_unique<bool>(true));
                REQUIRE(v.size() == i);
            }
        }
        {
            struct DefaultDeleted {
                DefaultDeleted() = delete;
                ~DefaultDeleted() = default;
                DefaultDeleted(int n) : num(n) {}
                int num;
            };
            using Vec = sh::ArrayVector<DefaultDeleted, 10>;
            Vec v;
            REQUIRE(v.size() == 0);
            for (int i = 1; i < 10; ++i) {
                v.push_back(DefaultDeleted(i));
                REQUIRE(v.back().num == i);
            }
        }
    }
    
    SECTION("pop_back resize clear") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        
        Vec v(5, ptr);
        v.pop_back();
        REQUIRE(v.size() == 4);
        REQUIRE(ptr.use_count() == 5);
        
        v.resize(8, ptr);
        REQUIRE(v.size() == 8);
        REQUIRE(ptr.use_count() == 9);
        
        v.resize(9);
        REQUIRE(v.size() == 9);
        REQUIRE(ptr.use_count() == 9);
        
        v.resize(5, ptr);
        REQUIRE(v.size() == 5);
        REQUIRE(ptr.use_count() == 6);
        
        v.clear();
        REQUIRE(v.size() == 0);
        REQUIRE(ptr.use_count() == 1);
    }
    
    SECTION("emplace_back") {
        static int count;
        struct ManyParams {
            explicit ManyParams(int i, float f, double d) {}
            
            ManyParams(ManyParams&& o) { count++; }
            ManyParams(const ManyParams& o) { count++; }
            ManyParams operator=(const ManyParams& o) { count++; return *this; }
            ManyParams operator=(ManyParams&& o) { count++; return *this; }
        };
        
        count = 0;
        using Vec = sh::ArrayVector<ManyParams, 10>;
        Vec v;
        for (int i = 0; i < 10; ++i) {
            v.emplace_back(i, i * 1.f, i * 1.0);
            REQUIRE(v.size() == (i + 1));
        }
        REQUIRE(count == 0);
            
        v.pop_back();
        v.emplace_back(ManyParams(1, 1.f, 1.0));
        REQUIRE(count == 1);
    }
        
    SECTION("iterators") {
        using Vec = sh::ArrayVector<int, 10>;
        Vec v{2, 4, 5, 6, 3, 1, 0};
        std::sort(v.begin(), v.end());
        REQUIRE(v.size() == 7);
        for (int i = 1; i < v.size(); ++i) {
            REQUIRE(v[i] > v[i-1]);
        }
        std::rotate(v.begin(), v.begin() + 1, v.end());
        REQUIRE(v.back() == 0);
        for (int i = 0; i < v.size() - 1; ++i) {
            REQUIRE(v[i] == i+1);
        }
    }
        
    SECTION("erase") {
        using Vec = sh::ArrayVector<int, 10>;
        Vec v{0, 1, 2, 3, 4, 5, 6};
        
        v.erase(v.begin()+3);
        REQUIRE(v.size() == 6);
        REQUIRE(v == Vec{0, 1, 2, 4, 5, 6});
        
        v.erase(v.begin()+v.size()-1);
        REQUIRE(v == Vec{0, 1, 2, 4, 5});
        
        v.erase(v.begin());
        REQUIRE(v == Vec{1, 2, 4, 5});
        
        v.erase(v.begin()+1, v.begin()+2);
        REQUIRE(v == Vec{1, 4, 5});
        
        v.erase(v.begin(), v.end());
        REQUIRE(v.empty());
    }
    
    SECTION("swap") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptrT = std::make_shared<bool>(true);
        auto ptrF = std::make_shared<bool>(false);
        {
            Vec v0(2, ptrT);
            Vec v1(8, ptrF);
            REQUIRE(ptrT.use_count() == 3);
            REQUIRE(ptrF.use_count() == 9);
            std::for_each(v0.begin(), v0.end(), [](auto& val) { REQUIRE(*val == true); });
            std::for_each(v1.begin(), v1.end(), [](auto& val) { REQUIRE(*val == false); });
            
            swap(v0, v1);
            
            REQUIRE(ptrT.use_count() == 3);
            REQUIRE(ptrF.use_count() == 9);
            std::for_each(v0.begin(), v0.end(), [](auto& val) { REQUIRE(*val == false); });
            std::for_each(v1.begin(), v1.end(), [](auto& val) { REQUIRE(*val == true); });
            REQUIRE(v0.size() == 8);
            REQUIRE(v1.size() == 2);
        }
        REQUIRE(ptrF.unique());
        REQUIRE(ptrT.unique());
    }
        
    SECTION("Copy Assignment") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        SECTION("shorten") {
            Vec v0(4, ptr);
            Vec v1(3, ptr);
            REQUIRE(ptr.use_count() == 8);
            
            v0 = v1;
            REQUIRE(ptr.use_count() == 7);
            REQUIRE(v0.size() == v1.size());
            REQUIRE(v0.size() == 3);
        }
        REQUIRE(ptr.use_count() == 1);
        
        SECTION("lengthen") {
            Vec v0(4, ptr);
            Vec v1(3, ptr);
            REQUIRE(ptr.use_count() == 8);
            
            v1 = v0;
            REQUIRE(ptr.use_count() == 9);
            REQUIRE(v0.size() == v1.size());
            REQUIRE(v0.size() == 4);
        }
    }
    
    SECTION("Move Assignment") {
        using Vec = sh::ArrayVector<std::shared_ptr<bool>, 10>;
        auto ptr = std::make_shared<bool>();
        SECTION("shorten") {
            Vec v0(4, ptr);
            Vec v1(3, ptr);
            REQUIRE(ptr.use_count() == 8);
            
            v0 = std::move(v1);
            REQUIRE(ptr.use_count() == 4);
            REQUIRE(v0.size() == 3);
            REQUIRE(v1.size() == 0);
        }
        REQUIRE(ptr.use_count() == 1);
        
        SECTION("lengthen") {
            Vec v0(4, ptr);
            Vec v1(3, ptr);
            REQUIRE(ptr.use_count() == 8);
            
            v1 = std::move(v0);
            REQUIRE(ptr.use_count() == 5);
            REQUIRE(v0.size() == 0);
            REQUIRE(v1.size() == 4);
        }
    }
}
