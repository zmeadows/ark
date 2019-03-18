#pragma once

#include "ark/types.hpp"
#include "ark/third_party/skarupke/bytell_hash_map.hpp"

#include <array>
#include <bitset>
#include <iostream>
#include <optional>
#include <tuple>

namespace ark {

template <typename AllComponents, typename AllSystems, typename AllResources = TypeList<>>
class World {
    static_assert(detail::is_specialization<AllComponents, TypeList>::value);
    static_assert(detail::is_specialization<AllSystems, TypeList>::value);
    static_assert(detail::is_specialization<AllResources, TypeList>::value);

    // ------------------------------------------------------------------------------------

    ComponentStash<AllComponents> m_component_stash;
    ResourceStash<AllResources> m_resource_stash;
    SystemStash<AllSystems> m_system_stash;

    // ------------------------------------------------------------------------------------

    using ComponentMask = BitMask<AllComponents::size>;
    std::array<ComponentMask, AllSystems::size> m_system_masks;
    ska::bytell_hash_map<EntityID, ComponentMask> m_component_masks;

    // ------------------------------------------------------------------------------------

    template <typename T> requires System<T>
    static constexpr size_t system_index(void) {
        return detail::index_in_type_list<T, AllSystems>();
    }

    template <typename T> requires System<T>
    void post_process_system_data(void) {
    }

    template <typename T> requires System<T>
    void run_system(void) {
        if (m_system_stash.template is_active<T>()) {
            typename T::SystemData* data = m_system_stash.template get_system_data<T>();
            T::run(*data);
            post_process_system_data<T>();
        }
    }

    template <typename... Ts>
    void run_all_systems(const TypeList<Ts...>&) {
        (run_system<Ts>(), ...);
    }

    // ------------------------------------------------------------------------------------

    // TODO: name template types more clearly here
    template <typename T>
    auto build_system_data_member(const size_t system_index) {
        if constexpr (detail::is_specialization<T, ReadComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());
        } else if constexpr (detail::is_specialization<T, WriteComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());
        } else if constexpr (std::is_same<T, FollowedEntities>::value) {
            return FollowedEntities(m_system_stash.get_followed_entities(system_index));
        } else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            return EntityBuilder<AllComponents>(&m_component_stash);
        } else if constexpr (detail::is_specialization<T, ReadResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());
        } else if constexpr (detail::is_specialization<T, WriteResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());
        } else {
            static_assert(detail::unreachable<T>::value, "Invalid system data type requested!");
        }
    }

    template<typename... Ts>
    std::tuple<Ts...> build_system_data(const size_t system_index, detail::type_tag<std::tuple<Ts...>>) {
        return std::make_tuple<Ts...>(build_system_data_member<Ts>(system_index)...);
    }

    template <typename T> requires System<T>
    void initialize() {
        using SD = typename T::SystemData;
        auto new_data = new SD(build_system_data(system_index<T>(), detail::type_tag<SD>()));
        m_system_stash.template set_system_data<T>(new_data);
    }

    template <typename... Ts>
    void initialize_system_data(const TypeList<Ts...>&) {
        (initialize<Ts>(),...);
    }

    inline bool validate(void) { return m_resource_stash.validate(); }

    World() = default;

public:
    World(const World&)             = delete;
    World(World&&)                  = delete;
    World& operator=(const World&)  = delete;
    World& operator=(World&&)       = delete;
    ~World(void)                    = default;

    template <typename Callable>
    static World* init(Callable&& resource_initializer) {
        World* new_world = new World();
        resource_initializer(&new_world->m_resource_stash);
        new_world->initialize_system_data(AllSystems());

        if (new_world->validate()) {
            return new_world;
        } else {
            return nullptr;
        }
    }

    inline void tick(void) { run_all_systems(AllSystems()); }

    template <typename T>
    inline T* get_resource(void) {
        return m_resource_stash.template get<T>();
    }


};

} // end namespace ark
