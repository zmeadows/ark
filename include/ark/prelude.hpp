#pragma once

#include <assert.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "ark/log.hpp"
#include "ark/type_list.hpp"

#ifndef NDEBUG
#   define ARK_ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)
#else
#   define ARK_ASSERT(condition, message) do { } while (false)
#endif

namespace ark {

using EntityID = uint32_t;

EntityID next_entity_id(void) {
    static std::atomic<EntityID> next_id = 2;
    return next_id.fetch_add(1);
}

std::string entities_to_string(const std::vector<EntityID>& entities) {
    std::stringstream ss;
    for (auto id : entities) ss << id << " ";
    return ss.str();
}

namespace detail {

template <class T>
constexpr std::string_view type_name()
{
    using namespace std;
#ifdef __clang__
    string_view p = __PRETTY_FUNCTION__;
    return string_view(p.data() + 34, p.size() - 34 - 1);
#elif defined(__GNUC__)
    string_view p = __PRETTY_FUNCTION__;
#  if __cplusplus < 201402
    return string_view(p.data() + 36, p.size() - 36 - 1);
#  else
    return string_view(p.data() + 62, p.find(';', 62) - 62);
#  endif
#elif defined(_MSC_VER)
    string_view p = __FUNCSIG__;
    return string_view(p.data() + 84, p.size() - 84 - 7);
#endif
}

template<typename T> struct type_tag {};

template <typename... T>
struct unreachable { static constexpr bool value = false; };

template< class T, class U >
concept bool SameHelper = std::is_same_v<T, U>;

template< class T, class U >
concept bool Same = detail::SameHelper<T, U> && detail::SameHelper<U, T>;

} // end namespace ark::detail
} // end namespace ark

