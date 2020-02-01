//
//  ArrayVector.Test.cpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 12/10/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#include <catch2/catch.hpp>

#include "ArrayVector.hpp"
#include "NonCopyable.h"
#include "NonMovable.h"

#include <memory>

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
            NoInit() { REQUIRE(false); }
            ~NoInit() { REQUIRE(false); }
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
    }
}

TEST_CASE("[ArrayVector] affordances") {
    SECTION("push_back") {
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
}
