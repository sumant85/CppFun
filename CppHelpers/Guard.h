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
constexpr int SizeInBytes() {
    using D = std::decay_t<T>;
    return sizeof(D);
}
    
template <typename T>
constexpr bool IsTrivialGuardCallable() {
    using D = std::decay_t<T>;
    return alignof(void*) % alignof(D) == 0 && std::is_trivially_destructible_v<D>;
}

// This class acts as a container for a callable that is trivially
// destructible so that we can store it in a type-erased container.
// This saves binary size by avoiding multiple instantiations of the
// template for each target.
template <int SizeInBytes>
class TrivialGuard : public GuardBase, NonCopyable, NonMovable {
public:
    template <typename Target, typename = std::enable_if_t<!std::is_lvalue_reference_v<Target>>>
    TrivialGuard(Target&& t) {
        using D = std::decay_t<Target>;
        new (&storage_) D(std::forward<Target>(t));
        // The trampoline mustn't have any captures since otherwise we cannot cast
        // this lambda to a function ptr and instead need to store it as a
        // std::fuction member variable which causes a large code bloat (due to vtables
        // and other template instantiations)
        // Also note how we remember D using the trampoline
        trampoline_ = [](void * ptr) { (*static_cast<D*>(ptr))(); };
        static_assert(noexcept(t()), "Cannot create guard with a target that can throw");
    }
    
    ~TrivialGuard() {
        trampoline_(&storage_);
    }
    
    void dismiss() override final {
        trampoline_ = [](void *) {};
    }
    
private:
    void(*trampoline_)(void *);
    std::aligned_storage<SizeInBytes, alignof(void*)> storage_;
};
    
template <typename T>
TrivialGuard(T&& t)->TrivialGuard<SizeInBytes<T>()>;
  
// For use cases where we cannot use the trivial guard above, we resort to
// instantiating a template for the target (which leads to code bloat)
template<typename Target>
class Guard : public GuardBase, NonCopyable, NonMovable {
public:
    constexpr Guard(Target&& target) : target_(std::move(target)), active_(true) {
        static_assert(noexcept(target()), "Cannot create guard with a target that can throw");
    }
    
    ~Guard() {
        if (active_) {
            target_();
        }
    }
    
    void dismiss() override final {
        active_ = false;
    }
    
private:
    Target target_;
    bool active_;
};
  
using GuardKey = std::unique_ptr<GuardBase>;

template <typename T>
auto makeGuard(T&& target) {
    using D = std::decay_t<T>;
    if constexpr (IsTrivialGuardCallable<D>()) {
        return GuardKey(new TrivialGuard(std::forward<T>(target)));
    } else {
        return GuardKey(new Guard<D>(std::forward<T>(target)));
    }
}
}
