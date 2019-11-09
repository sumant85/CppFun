//
//  Header.h
//  CppHelpers
//
//  Created by Sumant Hanumante on 8/11/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

#include "NonCopyable.h"
#include "NonMovable.h"

namespace sh {
// This class wraps around a target that would be called when the guard exits scope.
// Typical use is to perform cleanup as some function exits scope.
// Example :
// void foo(Database& db) {
//     auto conn = db.open();
//     Guard g([&]{ conn.close() });
//     // do stuff with conn
// }
// This class has a noexcept(false) destructor, which means that we allow throwing
// as a part of the guard cleanup phase.
// Instantiating this within a function body with compiler optimizations enabled leads
// to the same code as manually writing try/catch. Thus, we don't have to worry about
// generation of additional typeinfo/vtables which cause code bloat.
// Meant for scope-based cleanup
template<typename Target>
class StackGuard : NonCopyable {
public:
    constexpr StackGuard(Target&& target) : target_(std::move(target)), active_(true) {}
    
    ~StackGuard() noexcept(false) {
        static_assert(std::is_nothrow_destructible_v<Target>,
                      "So that destr doesn't throw to prevent multiple exceptions in flight "
                      "which would lead to the thrown exception being the one created by ~Target");
        if (active_) {
            target_();
        }
    }

    void dismiss() {
        active_ = false;
    }
    
private:
    bool active_;
    Target target_;
};
    
// If we need the ability to use the guards as class members without knowing what
// target they would be created with, we need to have some common base class. The
// drawback of this is the introduction of vtables for every member that derives
// from this, which leads to binary size bloat.
class GuardBase {
public:
    constexpr GuardBase() = default;
    virtual ~GuardBase() {};
    virtual void dismiss() = 0;
};
    
template <typename T>
constexpr size_t SizeInBytes() {
    using D = std::decay_t<T>;
    return sizeof(D);
}

// This class acts as a container for a callable that is stored in a type-erased container.
// This saves binary size by avoiding multiple instantiations of the template for each target.
// The requirement is that the target's operator() should be noexcept because that allows us
// to destroy the type-erased target without leaks.
template <size_t SizeInBytes, size_t Alignment>
class Guard : public GuardBase, NonCopyable, NonMovable {
public:
    template <typename Target, typename = std::enable_if_t<!std::is_lvalue_reference_v<Target>>>
    Guard(Target&& t) {
        using D = std::decay_t<Target>;
        new (&storage_) D(std::forward<Target>(t));
        // The trampoline mustn't have any captures since otherwise we cannot cast
        // this lambda to a function ptr and instead need to store it as a
        // std::fuction member variable which causes a large code bloat (due to vtables
        // and other template instantiations)
        // Also note how we remember D using the trampoline
        trampoline_ = [](void * ptr) noexcept(true) {
            auto& target = *static_cast<D*>(ptr);
            target();
            target.~D();
        };
        static_assert(noexcept(t()), "Cannot create guard with a target that can throw");
    }
    
    ~Guard() {
        if (trampoline_) {
            trampoline_(&storage_);
        }
    }
    
    void dismiss() override final {
        // This decision needs more thought. On one hand, we require an unnecessary if check in ~Guard.
        // The other option is to say trampoline_ = [](void *) {}; // ie no-op
        // In that case, a dismissed guard has to pay for a much more expensive function call to
        // the trampoline. The reason we chose to use if in ~Guard is that if dismiss is
        // uncommon, then the branch predictor would be accurate most of the time, otherwise
        // dismiss is common and we save the cost of jumping to cold memory for dismissed guards.
        trampoline_ = nullptr;
    }
    
private:
    void(*trampoline_)(void *);
    std::aligned_storage<SizeInBytes, Alignment> storage_;
};
    
template <typename T>
Guard(T&& t)->Guard<SizeInBytes<T>(), alignof(T)>;
  
using GuardKey = std::unique_ptr<GuardBase>;

template <typename T>
auto makeGuard(T&& target) {
    return GuardKey(new Guard(std::forward<T>(target)));
}
}
