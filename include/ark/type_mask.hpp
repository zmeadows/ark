#pragma once

#include "ark/prelude.hpp"
#include "ark/third_party/bitset2/bitset2.hpp"

namespace ark {

template <typename Types>
class TypeMask {
    static_assert(detail::is_specialization<Types, TypeList>::value
                  && "Template parameter for TypeMask must be a TypeList.");

    Bitset2::bitset2<Types::size> m_bitset;

    template <typename T>
    static constexpr size_t index(void) {
        return detail::index_in_type_list<T, Types>();
    }

public:
    //TODO: create templated set/unset/check methods
    inline constexpr void set(size_t bit) { m_bitset.set(bit); }
    inline constexpr void unset(size_t bit) { m_bitset.reset(bit); }
    inline constexpr bool check(size_t bit) { return m_bitset.test(bit); }
    inline constexpr bool is_subset_of(const TypeMask<Types>& other) const {
        return (m_bitset & other.m_bitset) == m_bitset;
    }

    friend bool operator==(const TypeMask<Types>& a, const TypeMask<Types>& b) {
        return a.get_bitset() == b.get_bitset();
    }

    constexpr TypeMask(void) : m_bitset() {}

    template <typename... Ts>
    constexpr TypeMask(const TypeList<Ts...>&) : TypeMask() {
        (set(index<Ts>()), ...);
    }

    inline const Bitset2::bitset2<Types::size>& get_bitset(void) const {
        return m_bitset;
    }
};

// bare-bones information about which components an entity posesses
template <typename AllComponents>
struct EntitySpec {
    EntityID id;
    TypeMask<AllComponents> mask;
};

} // end namespace ark

namespace std {

template <typename Types>
struct hash<ark::TypeMask<Types>> {
    inline size_t operator()(const ark::TypeMask<Types>& mask) const {
        return std::hash<Bitset2::bitset2<Types::size>>{}(mask.get_bitset());
    }
};

} // end namespace std

