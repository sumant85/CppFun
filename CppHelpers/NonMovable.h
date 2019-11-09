//
//  NonMovable.h
//  CppHelpers
//
//  Created by Sumant Hanumante on 8/26/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

namespace sh {
class NonMovable {
public:
    NonMovable() = default;
    NonMovable(NonMovable&& other) = delete;
    NonMovable& operator=(NonMovable&& other) = delete;
};
}