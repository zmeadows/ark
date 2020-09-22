#pragma once

#include <limits>

namespace ark {

template<typename... Ts>
struct TypeList {
    static constexpr size_t size{ sizeof... (Ts) };
    constexpr TypeList() = default; // necessary?
};

namespace detail {

template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};


template<typename>
constexpr std::size_t locate(std::size_t) {
    return std::numeric_limits<size_t>::max();
}

template<typename IndexedType, typename T, typename... Ts>
constexpr std::size_t locate(std::size_t ind = 0) {
    if (std::is_same<IndexedType, T>::value) {
        return ind;
    } else {
        return locate<IndexedType, Ts...>(ind + 1);
    }
}

template<typename T, typename>
struct IndexOf;

template<typename T, typename... ListedTypes>
struct IndexOf<T, TypeList<ListedTypes...>>
  : std::integral_constant<std::size_t, locate<T, ListedTypes...>()>
{};

}

namespace type_list {

template <typename T, typename List>
static constexpr size_t index(void) {
    static_assert(detail::is_specialization<List, TypeList>::value);
    constexpr size_t idx = detail::IndexOf<T, List>::value;
    static_assert(idx < List::size);
    return idx;
}

}

}
