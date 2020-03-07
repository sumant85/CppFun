//
//  ArrayVector.hpp
//  CppHelpers
//
//  Created by Sumant Hanumante on 12/9/19.
//  Copyright © 2019 Sumant Hanumante. All rights reserved.
//

#pragma once

#include <array>
#include <cassert>
#include <exception>
#include <type_traits>

namespace sh {
namespace detail {
#define ARRAY_VECTOR_STORAGE_INTERNALS \
public: \
protected: \
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>; \
    std::array<Storage, Capacity> storage_; \
    std::size_t size_ = 0; \
private: \
    friend Derived; \
    ArrayVectorStorage() = default;

// This class is used to conditionally enable the type trait for trivial destruction.
// It uses CRTP to call destroy on Derived if type is non-trivial. Note that we must store
// all the data members in base class, since the derived class has already been destroyed
// by the time the base class destructor gets called.
// Also, note that since we use CRTP, we need to access the base members using `this->` in
// the derived class.
template <typename Derived, typename T, std::size_t Capacity, bool TrivialDestr = false>
class ArrayVectorStorage {
    ARRAY_VECTOR_STORAGE_INTERNALS
public:
    
    // The destructor is marked noexcept because if we throw during
    // destruction, then we can potentially leak the contents. In such a case, we just
    // choose to terminate than leak.
    ~ArrayVectorStorage() noexcept {
        static_cast<Derived&>(*this).clear();
    }
};

template <typename Derived, typename T, std::size_t Capacity>
class ArrayVectorStorage<Derived, T, Capacity, true> {
    ARRAY_VECTOR_STORAGE_INTERNALS
};

#undef ARRAY_VECTOR_STORAGE_INTERNALS
}


#define CRTP_BASE \
detail::ArrayVectorStorage<ArrayVector<T, Capacity, PerformBoundsCheck>, \
                           T, Capacity, std::is_trivially_destructible_v<T>>

template <typename T, std::size_t Capacity, bool PerformBoundsCheck = false>
class ArrayVector : public CRTP_BASE {
#undef CRTP_BASE
public:
    ArrayVector() = default;
  
    explicit constexpr ArrayVector(std::size_t initialSize, const T& defaultValue) noexcept(NTCC) : ArrayVector() {
        assert(initialSize <= capacity_);
        expand(initialSize, defaultValue);
    }
    
    constexpr ArrayVector(std::size_t initialSize) noexcept(NTDC) : ArrayVector() {
        assert(initialSize <= capacity_);
        expand(initialSize);
    }
    
    // If a copy construction throws, state of *this is undefined.
    // other is unmodified.
    // The delegating constructor guarantees that detructor is called,
    // so there are no leaks.
    constexpr ArrayVector(const ArrayVector& other) noexcept(NTCC) : ArrayVector(other.begin(), other.end()) {}
    
    // If a move construction throws, state of *this and other is undefined.
    // The delegating constructor guarantees that detructor is called,
    // so there are no leaks.
    constexpr ArrayVector(ArrayVector&& other) noexcept(NTMC) : ArrayVector() {
        if constexpr (std::is_trivially_move_constructible_v<T>) {
            auto ptr = reinterpret_cast<T*>(this->storage_.data());
            auto ptrOther = reinterpret_cast<const T*>(other.storage_.data());
            
            std::memcpy(ptr, ptrOther, sizeof(T) * other.size_);
            this->size_ = other.size_;
        } else {
            while (this->size_ < other.size()) {
                push_back(std::move(other[this->size_]));
            }
        }
        
        // To give an API similar to vector, also clear out other so that
        // size() will return 0
        other.clear();
    }
    
    template <typename U>
    constexpr ArrayVector(std::initializer_list<U> il)
    noexcept(std::is_nothrow_constructible_v<T, U>) {
        assert(il.size() <= capacity_);
        for (auto it = il.begin(); it != il.end(); ++it) {
            emplace_back(std::move(*it));
        }
    }
    
    // Exception safety: No leaks, input remains unmodified.
    // *this continues to be valid on exception, but existing state maybe be modified
    // Copy and swap is expensive here since swap is also a O(n) operation, so instead
    // we assume there is no aliasing.
    constexpr ArrayVector& operator=(const ArrayVector& other) noexcept(NTCA && NTCC && NTD) {
        shorten(other.size());
        std::size_t idx = 0;
        while (idx < this->size_) {
            (*this)[idx] = other[idx];
            idx++;
        }
        while (this->size_ < other.size()) {
            push_back(other[this->size_]);
        }
        
        return *this;
    }
    
    // Exception safety: No leaks, both *this and other can be in modified states.
    constexpr ArrayVector& operator=(ArrayVector&& other) noexcept(NTMA && NTD) {
        shorten(other.size());
        std::size_t idx = 0;
        while (idx < this->size_) {
            (*this)[idx] = std::move(other[idx]);
            idx++;
        }
        while (this->size_ < other.size()) {
            push_back(std::move(other[this->size_]));
        }
        
        // To give an API similar to vector, also clear out other so that
        // size() will return 0
        other.clear();
        return *this;
    }

    constexpr void resize(std::size_t toSize, const T& defaultValue) noexcept(NTC && NTD) {
        if (toSize < this->size_) {
            shorten(toSize);
        } else {
            expand(toSize, defaultValue);
        }
    }
    
    constexpr void resize(std::size_t toSize) noexcept(NTC && NTD) {
        if (toSize < this->size_) {
            shorten(toSize);
        } else {
            expand(toSize);
        }
    }
    
    constexpr void push_back(const T& val) noexcept(noexcept(emplace_back(val))) {
        emplace_back(val);
    }
    
    constexpr void push_back(T&& val) noexcept(noexcept(emplace_back(std::move(val)))) {
        emplace_back(std::move(val));
    }
    
    template <typename... Args>
    constexpr void emplace_back(Args&&... args) noexcept(noexcept(checkSize()) &&
                                                         std::is_nothrow_constructible_v<T, Args...>) {
        checkSize();
        new (&this->storage_[this->size_]) T(std::forward<Args>(args)...);
        // Only increment size after T is constructed so that in case an exception is thrown,
        // we would only destroy elements from [0, this->size_)
        this->size_++;
    }
    
    constexpr void pop_back() noexcept(NTD) {
        if constexpr (std::is_trivially_destructible<T>()) {
            this->size_--;
        } else {
            reinterpret_cast<T&>(this->storage_[--this->size_]).~T();
        }
    }
    
    constexpr T& back() noexcept {
        return reinterpret_cast<T&>(this->storage_[this->size_ - 1]);
    }
    constexpr const T& back() const noexcept {
        return reinterpret_cast<const T&>(this->storage_[this->size_ - 1]);
    }
    
    constexpr T& front() noexcept {
        return reinterpret_cast<T&>(this->storage_[0]);
    }
    constexpr const T& front() const noexcept {
        return reinterpret_cast<const T&>(this->storage_[0]);
    }
    
    constexpr std::size_t size() const noexcept {
        return this->size_;
    }
    
    constexpr std::size_t capacity() const noexcept {
        return capacity_;
    }
    
    constexpr bool empty() const noexcept {
        return this->size_ == 0;
    }
    
    constexpr void clear() noexcept(NTD) {
        shorten(0);
    }
    
    constexpr T& operator[](std::size_t pos) noexcept {
        assert(pos >=0 && pos < capacity_);
        return reinterpret_cast<T&>(this->storage_[pos]);
    }
    
    constexpr const T& operator[](std::size_t pos) const noexcept {
        assert(pos >=0 && pos < capacity_);
        return reinterpret_cast<const T&>(this->storage_[pos]);
    }
    
    constexpr T* data() noexcept {
        return reinterpret_cast<T*>(this->storage_.data());
    }
    
    constexpr const T* data() const noexcept {
        return reinterpret_cast<const T*>(this->storage_.data());
    }

    // Copied from std::array :(
    typedef T                                     value_type;
    typedef value_type&                           reference;
    typedef const value_type&                     const_reference;
    typedef value_type*                           iterator;
    typedef const value_type*                     const_iterator;
    typedef value_type*                           pointer;
    typedef const value_type*                     const_pointer;
    typedef std::size_t                           size_type;
    typedef ptrdiff_t                             difference_type;
    typedef std::reverse_iterator<iterator>       reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    constexpr auto begin() noexcept {return iterator(this->storage_.data());}
    constexpr auto begin() const noexcept {return const_iterator(this->storage_.data());}
    constexpr auto end() noexcept {return iterator(this->storage_.data() + this->size_);}
    constexpr auto end() const noexcept {return const_iterator(data() + this->size_);}

    constexpr auto rbegin() noexcept {return reverse_iterator(end());}
    constexpr auto rbegin() const noexcept {return const_reverse_iterator(end());}
    constexpr auto rend() noexcept {return reverse_iterator(begin());}
    constexpr auto rend() const noexcept {return const_reverse_iterator(begin());}
    
    constexpr auto cbegin() const noexcept {return begin();}
    constexpr auto cend() const noexcept {return end();}
    constexpr auto crbegin() const noexcept {return rbegin();}
    constexpr auto crend() const noexcept {return rend();}
    
    constexpr ArrayVector(const_iterator obegin, const_iterator oend) noexcept(NTDC) : ArrayVector() {
        if constexpr (std::is_trivially_constructible_v<T>) {
            std::copy(obegin, oend, begin());
            this->size_ = (oend - obegin);
        } else {
            for (auto it = obegin; it != oend; ++it) {
                emplace_back(*it);
            }
        }
    }
    
    // Exception safety: No leaks, but array might be left in invalid state if exception
    // is thrown while moving elements to account for the erased element.
    constexpr void erase(const_iterator begin) noexcept(NTD && NTMA) {
        erase(begin, begin + 1);
    }
    
    // Erase from inclusive and to exclusive
    // Exception safety: No leaks, but array might be left in invalid state if exception
    // is thrown while erasing.
    constexpr void erase(const_iterator from, const_iterator to) noexcept(NTD && NTMA) {
        auto first = const_cast<iterator>(from);
        auto last = const_cast<iterator>(to);
        // Instead of rotate, we move construct only the requied elements
        while (last < end()) {
            *first++ = std::move(*last++);
        }
        shorten(size() - (last - first));
    }
    
    constexpr friend bool operator==(const ArrayVector& l, const ArrayVector& r) noexcept {
        return std::equal(l.begin(), l.end(), r.begin());
    }
    
    constexpr friend bool operator!=(const ArrayVector& l, const ArrayVector& r) noexcept {
        return !(l == r);
    }
    
    constexpr friend void swap(ArrayVector& a1, ArrayVector& a2) {
        ArrayVector& smaller = a1;
        ArrayVector& larger = a2;
        if (a1.size() > a2.size()) {
            std::swap(smaller, larger);
        }
        std::swap_ranges(smaller.begin(), smaller.end(), larger.begin());
        
        const auto smallerSize = smaller.size();
        std::for_each(larger.begin() + smallerSize, larger.end(), [&](auto& val) {
            smaller.push_back(std::move(val));
        });
        larger.shorten(smallerSize);
    }
    
private:
    static constexpr auto CC = std::is_copy_constructible_v<T>;
    static constexpr auto MC = std::is_move_constructible_v<T>;
    static constexpr auto NTC = std::is_nothrow_constructible_v<T>;
    static constexpr auto NTD = std::is_nothrow_destructible_v<T>;
    static constexpr auto NTS = std::is_nothrow_swappable_v<T>;
    static constexpr auto NTCA = std::is_nothrow_copy_assignable_v<T>;
    static constexpr auto NTCC = std::is_nothrow_copy_constructible_v<T>;
    static constexpr auto NTDC = std::is_nothrow_default_constructible_v<T>;
    static constexpr auto NTMA = std::is_nothrow_move_assignable_v<T>;
    static constexpr auto NTMC = std::is_nothrow_move_constructible_v<T>;
    static constexpr auto capacity_ = Capacity;
    
    constexpr void shorten(std::size_t toSize) noexcept(NTD) {
        if constexpr (std::is_trivially_destructible_v<T>) {
            this->size_ = toSize;
        } else {
            while (toSize < this->size_) {
                pop_back();
            }
        }
    }
    
    constexpr void expand(std::size_t toSize, const T& value) noexcept(NTC) {
        if constexpr (std::is_trivially_constructible_v<T>) {
            auto begin = reinterpret_cast<T*>(this->storage_.data()) + this->size_;
            auto end = begin + (toSize - this->size_) + 1;
            std::fill(begin, end, value);
            this->size_ = toSize;
        } else {
            while (this->size_ < toSize) {
                emplace_back(value);
            }
        }
    }
    
    constexpr void expand(std::size_t toSize) noexcept(NTC) {
        if constexpr (std::is_trivially_constructible_v<T>) {
            auto begin = reinterpret_cast<T*>(this->storage_.data()) + this->size_;
            auto end = begin + (toSize - this->size_) + 1;
            std::fill(begin, end, T{});
            this->size_ = toSize;
        } else {
            while (this->size_ < toSize) {
                emplace_back();
            }
        }
    }
    
    constexpr void checkSize() noexcept(!PerformBoundsCheck) {
        if constexpr (PerformBoundsCheck) {
            if (this->size_ >= capacity_) {
                // TODO: Which error to throw here?
                throw std::runtime_error("Capacity exceeded");
            }
        } else {
            assert(this->size_ < capacity_);
        }
    }
};
}
