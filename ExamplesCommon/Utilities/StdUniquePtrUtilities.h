//------------------------------------------------------------------------------
//! \file       ARAStdUniquePtrUtilities.h
//!             convenience functions to assist with operations related
//!             to std::unique_ptr (comparison, searching in vectors)
//!             Also provides an implementation of std::make_unique for older compilers.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2026, Celemony Software GmbH, All Rights Reserved.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------

#ifndef StdUniquePtrUtilities_h
#define StdUniquePtrUtilities_h

#include <vector>
#include <algorithm>
#include <memory>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ARA {

/*******************************************************************************/
// helper: std::unique_ptr comparison predicate

template <typename T>
struct unique_ptr_compare
{
    const T* ptr;
    bool operator () (const std::unique_ptr<T>& uPtr) noexcept
    {
        return uPtr.get () == ptr;
    }
};

/*******************************************************************************/
// Find an a raw pointer inside a vector of std::unique_ptrs in the vector and,
// if found, erase it from the vector. Returns true if found & erased, otherwise false
template <typename T, typename U, typename std::enable_if<std::is_convertible<T*, U>::value || std::is_convertible<U, T*>::value, bool>::type = true>
inline bool find_erase (std::vector<std::unique_ptr<T>>& container, const U& ptr)
{
    auto it = std::find_if (container.begin (), container.end (), unique_ptr_compare<T>{ ptr });
    if (it == container.end ())
        return false;

    container.erase (it);
    return true;
}

/*******************************************************************************/
// Determine if a raw pointer exists in a vector as a std::unique_ptr

template <typename T, typename U, typename std::enable_if<std::is_convertible<T*, U>::value || std::is_convertible<U, T*>::value, bool>::type = true>
inline bool contains (std::vector<std::unique_ptr<T>> const& container, const U& ptr)
{
    return std::any_of (container.begin (), container.end (), unique_ptr_compare<T>{ ptr });
}

/*******************************************************************************/
// Find position of a raw pointer inside a vector of std::unique_ptrs.
template <typename T, typename U, typename std::enable_if<std::is_convertible<T*, U>::value || std::is_convertible<U, T*>::value, bool>::type = true>
inline intptr_t index_of (std::vector<std::unique_ptr<T>> const& container, const U& ptr)
{
    auto it = std::find_if (container.begin (), container.end (), unique_ptr_compare<T>{ ptr });
    if (it == container.end ())
        return -1;

    return std::distance (container.begin (), it);
}

}   // namespace ARA

/*******************************************************************************/
// Implement std::make_unique for pre-C++14 compilers, as published by ISO here:
// https://isocpp.org/files/papers/N3656.txt
#if !defined (_MSC_VER) && (__cplusplus < 201402L)
namespace std
{
    template<class T>
    struct _Unique_if
    {
        typedef unique_ptr<T> _Single_object;
    };
    template<class T>
    struct _Unique_if<T[]>
    {
        typedef unique_ptr<T[]> _Unknown_bound;
    };
    template<class T, size_t N>
    struct _Unique_if<T[N]>
    {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
    typename _Unique_if<T>::_Single_object make_unique (Args&&... args)
    {
        return unique_ptr<T> (new T (std::forward<Args> (args)...));
    }
    template<class T>
    typename _Unique_if<T>::_Unknown_bound make_unique (size_t n)
    {
        typedef typename remove_extent<T>::type U;
        return unique_ptr<T> (new U[n] ());
    }
    template<class T, class... Args>
    typename _Unique_if<T>::_Known_bound make_unique (Args&&...) = delete;
}   // namespace std
#endif // !defined (_MSC_VER) && (__cplusplus < 201402L)

#endif // StdUniquePtrUtilities_h
