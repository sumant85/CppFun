//
//  NonCopyable.h
//  CppHelpers
//
//  Created by Sumant Hanumante on 8/11/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

namespace sh {
class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable& other) = delete;
    NonCopyable& operator=(const NonCopyable& other) = delete;
};
}