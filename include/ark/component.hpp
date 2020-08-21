#pragma once

#include <array>
#include <concepts>

#include "ark/prelude.hpp"

namespace ark {

// clang-format off
template <typename S>
concept ComponentStorage = requires(S store, const S const_store, EntityID id)
{
    typename S::ComponentType; // each storage must define its component type

    { const_store.has(id) } -> std::same_as<bool>;
    { store.get(id) }       -> std::same_as<typename S::ComponentType&>;
    { const_store.get(id) } -> std::same_as<const typename S::ComponentType&>;
    { store.get_if(id) }    -> std::same_as<typename S::ComponentType*>;
    { store.attach(id) }    -> std::same_as<typename S::ComponentType&>;
    { store.detach(id) }    -> std::same_as<void>;
};

template <typename C>
concept Component =
    ComponentStorage<typename C::Storage>&& detail::Same<typename C::Storage::ComponentType, C>;
// clang-format on

// TODO: This class is very shallow and can likely be removed and the behavior placed in-line into
// ark::World
template <typename AllComponents>
class ComponentStash {
    std::array<void*, AllComponents::size> m_storage;

    template <Component T>
    void initialize()
    {
        auto ptr = new typename T::Storage();
        assert(ptr != nullptr && "Failed to allocate component storage");
        m_storage[component_index<T>()] = ptr;
    }

    template <typename... Ts>
    void initialize_component_storage(const TypeList<Ts...>&)
    {
        (initialize<Ts>(), ...);
    }

    template <Component T>
    void cleanup(void)
    {
        delete get<T>();
    }

    template <typename... Ts>
    void cleanup_component_storage(const TypeList<Ts...>&)
    {
        (cleanup<Ts>(), ...);
    }

public:
    template <Component T>
    inline typename T::Storage* get(void)
    {
        return static_cast<typename T::Storage*>(m_storage[component_index<T>()]);
    }

    template <Component T>
    inline const typename T::Storage* get(void) const
    {
        return static_cast<const typename T::Storage*>(m_storage[component_index<T>()]);
    }

    template <Component T>
    static constexpr size_t component_index(void)
    {
        return type_list::index<T, AllComponents>();
    }

    ComponentStash(void) : m_storage({nullptr}) { initialize_component_storage(AllComponents()); }

    ComponentStash(const ComponentStash&) = delete;
    ComponentStash(ComponentStash&&) = delete;
    ComponentStash& operator=(const ComponentStash&) = delete;
    ComponentStash& operator=(ComponentStash&&) = delete;

    ~ComponentStash(void) { cleanup_component_storage(AllComponents()); }
};

}  // end namespace ark

