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
template <typename T, typename U, std::enable_if_t<std::is_convertible_v<T*, U> || std::is_convertible_v<U, T*>, bool> = true>
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

template <typename T, typename U, std::enable_if_t<std::is_convertible_v<T*, U> || std::is_convertible_v<U, T*>, bool> = true>
inline bool contains (std::vector<std::unique_ptr<T>> const& container, const U& ptr)
{
    return std::any_of (container.begin (), container.end (), unique_ptr_compare<T>{ ptr });
}

/*******************************************************************************/
// Find position of a raw pointer inside a vector of std::unique_ptrs.
template <typename T, typename U, std::enable_if_t<std::is_convertible_v<T*, U> || std::is_convertible_v<U, T*>, bool> = true>
inline intptr_t index_of (std::vector<std::unique_ptr<T>> const& container, const U& ptr)
{
    auto it = std::find_if (container.begin (), container.end (), unique_ptr_compare<T>{ ptr });
    if (it == container.end ())
        return -1;

    return std::distance (container.begin (), it);
}

}   // namespace ARA

#endif // StdUniquePtrUtilities_h
