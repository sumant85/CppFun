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
template <typename T, size_t Capacity, bool PerformBoundsCheck = false>
class ArrayVector {
public:
    ArrayVector() = default;
  
    explicit constexpr ArrayVector(size_t initialSize, const T& defaultValue) noexcept(NTCC) : ArrayVector() {
        assert(initialSize <= capacity_);
        if constexpr (std::is_trivially_constructible_v<T>) {
            auto ptr = reinterpret_cast<T*>(storage_.data());
            std::fill(ptr, ptr + initialSize, defaultValue);
        } else {
            while (size_ < initialSize) {
                emplace_back(defaultValue);
            }
        }
    }
    
    constexpr ArrayVector(size_t initialSize) noexcept(NTDC) : ArrayVector() {
        while (size_ < initialSize) {
            emplace_back();
        }
    }
    
    // If a copy construction throws, state of *this is undefined.
    // other is unmodified.
    // The delegating constructor guarantees that detructor is called,
    // so there are no leaks.
    constexpr ArrayVector(const ArrayVector& other) noexcept(NTCC) : ArrayVector() {
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            auto ptr = reinterpret_cast<T*>(storage_.data());
            auto ptrOther = reinterpret_cast<const T*>(other.storage_.data());
            
            std::memcpy(ptr, ptrOther, sizeof(T) * other.size_);
            size_ = other.size_;
        } else {
            while (size_ < other.size()) {
                push_back(other[size_]);
            }
        }
    }
    
    // If a move construction throws, state of *this and other is undefined.
    // The delegating constructor guarantees that detructor is called,
    // so there are no leaks.
    constexpr ArrayVector(ArrayVector&& other) noexcept(NTMC) : ArrayVector() {
        if constexpr (std::is_trivially_move_constructible_v<T>) {
            auto ptr = reinterpret_cast<T*>(storage_.data());
            auto ptrOther = reinterpret_cast<const T*>(other.storage_.data());
            
            std::memcpy(ptr, ptrOther, sizeof(T) * other.size_);
            size_ = other.size_;
        } else {
            while (size_ < other.size()) {
                push_back(std::move(other[size_]));
            }
        }
        
        // To give an API similar to vector, also clear out other so that
        // size() will return 0
        other.clear();
    }
    
    // The destructor is marked noexcept because if we throw during
    // destruction, then we can potentially leak the contents. In such a case, we just
    // choose to terminate than leak
    ~ArrayVector() noexcept {
        clear();
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
        size_t idx = 0;
        while (idx < size_) {
            (*this)[idx] = other[idx];
            idx++;
        }
        while (size_ < other.size()) {
            push_back(other[size_]);
        }
        
        return *this;
    }
    
    // Exception safety: No leaks, both *this and other can be in modified states.
    constexpr ArrayVector& operator=(ArrayVector&& other) noexcept(NTMA && NTD) {
        shorten(other.size());
        size_t idx = 0;
        while (idx < size_) {
            (*this)[idx] = std::move(other[idx]);
            idx++;
        }
        while (size_ < other.size()) {
            push_back(std::move(other[size_]));
        }
        
        // To give an API similar to vector, also clear out other so that
        // size() will return 0
        other.clear();
        return *this;
    }

    constexpr void resize(size_t toSize, const T& defaultValue) noexcept(NTC && NTD) {
        if (toSize < size_) {
            shorten(toSize);
            return;
        }
        
        while (toSize > size_) {
            push_back(defaultValue);
        }
    }
    
    template <typename = std::is_default_constructible<T>>
    constexpr void resize(size_t toSize) noexcept(NTC && NTD) {
        resize(toSize, T{});
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
        new (&storage_[size_]) T(std::forward<Args>(args)...);
        // Only increment size after T is constructed so that in case an exception is thrown,
        // we would only destroy elements from [0, size_)
        size_++;
    }
    
    constexpr void pop_back() noexcept(NTD) {
        if constexpr (std::is_trivially_destructible<T>()) {
            size_--;
        } else {
            reinterpret_cast<T&>(storage_[--size_]).~T();
        }
    }
    
    constexpr T& back() noexcept {
        return reinterpret_cast<T&>(storage_[size_ - 1]);
    }
    constexpr const T& back() const noexcept {
        return reinterpret_cast<const T&>(storage_[size_ - 1]);
    }
    
    constexpr T& front() noexcept {
        return reinterpret_cast<T&>(storage_[0]);
    }
    constexpr const T& front() const noexcept {
        return reinterpret_cast<const T&>(storage_[0]);
    }
    
    constexpr size_t size() const noexcept {
        return size_;
    }
    
    constexpr size_t capacity() const noexcept {
        return capacity_;
    }
    
    constexpr bool empty() const noexcept {
        return size_ == 0;
    }
    
    constexpr void clear() noexcept(NTD) {
        shorten(0);
    }
    
    constexpr T& operator[](size_t pos) noexcept {
        assert(pos >=0 && pos < capacity_);
        return reinterpret_cast<T&>(storage_[pos]);
    }
    
    constexpr const T& operator[](size_t pos) const noexcept {
        assert(pos >=0 && pos < capacity_);
        return reinterpret_cast<const T&>(storage_[pos]);
    }
    
    constexpr T* data() noexcept {
        return reinterpret_cast<T*>(storage_.data());
    }
    
    constexpr const T* data() const noexcept {
        return reinterpret_cast<const T*>(storage_.data());
    }

    // Copied from std::array :(
    typedef T                                     value_type;
    typedef value_type&                           reference;
    typedef const value_type&                     const_reference;
    typedef value_type*                           iterator;
    typedef const value_type*                     const_iterator;
    typedef value_type*                           pointer;
    typedef const value_type*                     const_pointer;
    typedef size_t                                size_type;
    typedef ptrdiff_t                             difference_type;
    typedef std::reverse_iterator<iterator>       reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    constexpr auto begin() noexcept {return iterator(storage_.data());}
    constexpr auto begin() const noexcept {return const_iterator(storage_.data());}
    constexpr auto end() noexcept {return iterator(storage_.data() + size_);}
    constexpr auto end() const noexcept {return const_iterator(data() + size_);}

    constexpr auto rbegin() noexcept {return reverse_iterator(end());}
    constexpr auto rbegin() const noexcept {return const_reverse_iterator(end());}
    constexpr auto rend() noexcept {return reverse_iterator(begin());}
    constexpr auto rend() const noexcept {return const_reverse_iterator(begin());}
    
    constexpr auto cbegin() const noexcept {return begin();}
    constexpr auto cend() const noexcept {return end();}
    constexpr auto crbegin() const noexcept {return rbegin();}
    constexpr auto crend() const noexcept {return rend();}
    
    constexpr ArrayVector(const_iterator begin, const_iterator end) noexcept(NTDC) : ArrayVector() {
        for (auto it = begin; it != end; ++it) {
            emplace_back(*it);
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
        resize(size() - (last - first));
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
    
    constexpr void shorten(size_t toSize) noexcept(NTD) {
        while (toSize < size_) {
            pop_back();
        }
    }
    
    constexpr void checkSize() noexcept(!PerformBoundsCheck) {
        if constexpr (PerformBoundsCheck) {
            if (size_ >= capacity_) {
                // TODO: Which error to throw here?
                throw std::runtime_error("Capacity exceeded");
            }
        } else {
            assert(size_ < capacity_);
        }
    }
    
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
    std::array<Storage, Capacity> storage_;
    size_t size_ = 0;
};
}
