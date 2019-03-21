#pragma once

#include <assert.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace ark {

using EntityID = uint32_t;

EntityID next_entity_id(void) {
    static std::atomic<EntityID> next_id = 2;
    return next_id.fetch_add(1);
}

template<typename... Ts>
struct TypeList {
    // using Self = TypeList<Ts...>;
    static constexpr size_t size{ sizeof... (Ts) };
    constexpr TypeList() = default; // necessary?
};

namespace detail {

// NOTE: this doesn't seem to work properly on templated types
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

//TODO: move into TypeList class
template<typename>
constexpr std::size_t locate(std::size_t) {
    return static_cast<std::size_t>(-1);
}

template<typename IndexedType, typename T, typename... Ts>
constexpr std::size_t locate(std::size_t ind = 0) {
    if (std::is_same<IndexedType, T>::value) {
        return ind;
    } else {
        return locate<IndexedType, Ts...>(ind + 1);
    }
}

template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

template<typename T, typename>
struct IndexOf;

template<typename T, typename... ListedTypes>
struct IndexOf<T, TypeList<ListedTypes...>>
  : std::integral_constant<std::size_t, locate<T, ListedTypes...>()>
{};

template <typename T, typename List>
static constexpr size_t index_in_type_list(void) {
    static_assert(detail::is_specialization<List, TypeList>::value);
    constexpr size_t idx = detail::IndexOf<T, List>::value;
    static_assert(idx < List::size);
    return idx;
}

} // end namespace ark::detail
} // end namespace ark

