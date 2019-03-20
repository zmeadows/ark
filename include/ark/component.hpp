#pragma once

#include "ark/prelude.hpp"

#include <array>

namespace ark {

template <typename S>
concept bool ComponentStorage = requires(S* store, const S* const_store, EntityID id) {
    typename S::ComponentType;
    { store->get(id)       } -> typename S::ComponentType&;
    { const_store->get(id) } -> const typename S::ComponentType&;
    { store->get_if(id)    } -> typename S::ComponentType*;
    { const_store->has(id) } -> bool;
    { store->attach(id)    } -> typename S::ComponentType&;
    { store->detach(id)    } -> void;
};

template <typename C>
concept bool Component = ComponentStorage<typename C::Storage>
                         && detail::Same<typename C::Storage::ComponentType, C>;

template <typename AllComponents>
class ComponentStash {
    std::array<void*, AllComponents::size> m_storage;

    template <typename T> requires Component<T>
    void initialize() {
        auto ptr = new typename T::Storage();
        assert(ptr && "Failed to allocate component storage");
        m_storage[component_index<T>()] = ptr;
    }

    template <typename... Ts>
    void initialize_component_storage(const TypeList<Ts...>&) {
        (initialize<Ts>(),...);
    }

    template <typename T> requires Component<T>
    void cleanup(void) { delete get<T>(); }

    template <typename... Ts>
    void cleanup_component_storage(const TypeList<Ts...>&) {
        (cleanup<Ts>(), ...);
    }

public:
    template <typename T> requires Component<T>
    inline typename T::Storage* get(void) {
        return static_cast<typename T::Storage*>(m_storage[component_index<T>()]);
    }

    template <typename T> requires Component<T>
    inline const typename T::Storage* get(void) const {
        return static_cast<const typename T::Storage*>(m_storage[component_index<T>()]);
    }

    template <typename T> requires Component<T>
    static constexpr size_t component_index(void) {
        return detail::index_in_type_list<T, AllComponents>();
    }

    ComponentStash(void) : m_storage({nullptr}) {
        initialize_component_storage(AllComponents());
    }

    ComponentStash(const ComponentStash&)            = delete;
    ComponentStash(ComponentStash&&)                 = delete;
    ComponentStash& operator=(const ComponentStash&) = delete;
    ComponentStash& operator=(ComponentStash&&)      = delete;

    ~ComponentStash(void) {
        cleanup_component_storage(AllComponents());
    }
};

} // end namespace ark

