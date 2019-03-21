#pragma once

#include "ark/prelude.hpp"
#include "ark/system.hpp"
#include "ark/resource.hpp"
#include "ark/flat_entity_set.hpp"
#include "ark/flat_hash_map.hpp"
#include "ark/third_party/ThreadPool.hpp"

#include <array>
#include <bitset>
#include <iostream>
#include <optional>
#include <thread>
#include <tuple>

namespace ark {

template < typename AllComponents
         , typename AllSystems
         , typename AllResources = TypeList<>
         >
class World {
    static_assert(detail::is_specialization<AllComponents, TypeList>::value);
    static_assert(detail::is_specialization<AllSystems, TypeList>::value);
    static_assert(detail::is_specialization<AllResources, TypeList>::value);

    template <typename T> requires System<T>
    static constexpr size_t system_index(void) {
        return detail::index_in_type_list<T, AllSystems>();
    }

    template <typename T> requires Component<T>
    static constexpr size_t component_index(void) {
        return detail::index_in_type_list<T, AllComponents>();
    }

    template <typename T>
    static constexpr size_t resource_index(void) {
        return detail::index_in_type_list<T, AllResources>();
    }

    // ------------------------------------------------------------------------------------

    //TODO: remove stashes
    ComponentStash<AllComponents> m_component_stash;
    ResourceStash<AllResources> m_resource_stash;

    // ------------------------------------------------------------------------------------

    std::array<bool, AllSystems::size> m_active;
    std::array<FlatEntitySet, AllSystems::size> m_followed;

    template <typename T> requires System<T>
    inline bool is_system_active(void) { return m_active[system_index<T>()]; }

    template <typename T> requires System<T>
    inline void set_system_active(void) { m_active[system_index<T>()] = true; }

    template <typename T> requires System<T>
    inline void set_system_inactive(void) { m_active[system_index<T>()] = false; }

    template <typename T> requires System<T>
    inline void toggle_system_active(void) { m_active[system_index<T>()] = !m_active[system_index<T>()]; }

    template <typename T> requires System<T>
    inline void follow_new_entities(const std::vector<EntityID>& entities) {
        m_followed[system_index<T>()].insert_new_entities(entities);
    }

    template <typename T> requires System<T>
    inline void follow_entities(const std::vector<EntityID>& entities) {
        m_followed[system_index<T>()].insert_entities(entities);
    }

    template <typename T> requires System<T>
    inline void unfollow_entities(const std::vector<EntityID>& entities) {
        m_followed[system_index<T>()].remove_entities(entities);
    }

    template <typename T> requires System<T>
    inline FollowedEntities get_followed_entities(void) {
        return FollowedEntities(std::addressof(m_followed[system_index<T>()])
                                , &m_thread_pool);
    }

    // TODO: remove this method
    inline FollowedEntities get_followed_entities(const size_t sys_idx) {
        return FollowedEntities(std::addressof(m_followed[sys_idx])
                                , &m_thread_pool);
    }

    // ------------------------------------------------------------------------------------

    using ComponentMask = TypeMask<AllComponents>;

    EntityMap<ComponentMask> m_entity_masks;

    template <typename T> requires System<T>
    static constexpr ComponentMask system_mask(void) {
        return ComponentMask(typename T::Subscriptions());
    }

    // ------------------------------------------------------------------------------------

    // After each system runs, and before the next system runs, we must react to any
    // created/removed components on pre-existing entities as well as any newly created
    // entities.  This primarily involves letting the relevant systems follow/unfollow
    // the new/modified entity.

    std::unordered_map<ComponentMask, std::vector<EntityID>> m_new_entity_roster;

    template <typename T> requires System<T>
    void alert_system_new_entities_created( const std::vector<EntityID>& new_entities
                                          , const ComponentMask& entity_mask)
    {
        static const ComponentMask sys_mask = system_mask<T>();
        if (sys_mask.is_subset_of(entity_mask)) {
            follow_new_entities<T>(new_entities);
        }
    }

    template <typename... SystemTypes>
    void alert_all_systems_new_entities_created( const std::vector<EntityID>& new_entities
                                               , const ComponentMask& entity_mask
                                               , const TypeList<SystemTypes...>&)
    {
        (alert_system_new_entities_created<SystemTypes>(new_entities, entity_mask), ...);
    }

    void post_process_newly_created_entities(void) {
        for (const auto& [mask, new_entities] : m_new_entity_roster) {

            if (!new_entities.empty()) {
                for (const EntityID id : new_entities) {
                    m_entity_masks.insert(id, mask);
                }

                alert_all_systems_new_entities_created(new_entities, mask, AllSystems());
            }
        }

        for (auto& it : m_new_entity_roster) {
            it.second.clear();
        }
    }

    // ------------------------------------------------------------------------------------

    std::array<std::vector<EntityID>, AllComponents::size> m_detach_component_updates;

    template <typename SystemType, typename ComponentType>
    requires System<SystemType> && Component<ComponentType>
    void alert_system_component_detached_from_entities(const std::vector<EntityID>& entities) {
        if constexpr (system_mask<SystemType>().check(component_index<ComponentType>())) {
            unfollow_entities<SystemType>(entities);
        }
    }

    template <typename ComponentType, typename... SystemTypes>
    requires Component<ComponentType>
    inline void _alert_all_systems_component_detached_from_entities(const std::vector<EntityID>& entities,
                                                                    const TypeList<SystemTypes...>&)
    {
        (alert_system_component_detached_from_entities<ComponentType, SystemTypes>(entities), ...);
    }

    template <typename T> requires Component<T>
    inline void alert_all_systems_component_detached_from_entities(const std::vector<EntityID>& entities)
    {
        _alert_all_systems_component_detached_from_entities<T>(entities, AllSystems());
    }

    template <typename T> requires Component<T>
    void post_process_newly_detached_component(const std::vector<EntityID>& entities)
    {
        for (const EntityID id : entities) {
            ComponentMask& entity_mask = m_entity_masks[id];
            entity_mask.unset(component_index<T>());
        }
        alert_all_systems_component_detached_from_entities<T>(entities);
    }

    template <typename T> requires Component<T>
    void post_process_newly_detached_components(void) {
        std::vector<EntityID>& updates = m_detach_component_updates[component_index<T>()];
        post_process_newly_detached_component<T>(updates);
        updates.clear();
    }

    // ------------------------------------------------------------------------------------

    std::array<std::vector<EntityID>, AllComponents::size> m_attach_component_updates;

    template <typename ComponentType, typename SystemType>
    requires System<SystemType> && Component<ComponentType>
    void alert_system_component_attached_to_entities(const std::vector<EntityID>& entities)
    {
        if constexpr (system_mask<SystemType>().check(component_index<ComponentType>())) {
            std::vector<EntityID> matched;
            matched.reserve(entities.size());
            for (const EntityID id : entities) {
                const ComponentMask& entity_mask = m_entity_masks[id];
                if (system_mask<SystemType>().is_subset_of(entity_mask)) {
                    matched.push_back(id);
                }
            }
            follow_entities<SystemType>(matched);
        }
    }

    template <typename ComponentType, typename... SystemTypes>
    inline void _alert_all_systems_component_attached_to_entities(const std::vector<EntityID>& entities,
                                                                  const TypeList<SystemTypes...>&)
    {
        (alert_system_component_attached_to_entities<ComponentType, SystemTypes>(entities), ...);
    }

    template <typename T> requires Component<T>
    inline void alert_all_systems_component_attached_to_entities(const std::vector<EntityID>& entities)
    {
        _alert_all_systems_component_attached_to_entities<T>(entities, AllSystems());
    }

    template <typename T> requires Component<T>
    void post_process_newly_attached_component(const std::vector<EntityID>& entities) {
        for (const EntityID id : entities) {
            ComponentMask& entity_mask = m_entity_masks[id];
            entity_mask.set(component_index<T>());
        }
        alert_all_systems_component_attached_to_entities<T>(entities);
    }

    template <typename T> requires Component<T>
    void post_process_newly_attached_components(void) {
        std::vector<EntityID>& updates = m_attach_component_updates[component_index<T>()];
        post_process_newly_attached_component<T>(updates);
        updates.clear();
    }

    // ------------------------------------------------------------------------------------

    template <typename T>
    void post_process_system_data_member(void) {
        if constexpr (  detail::is_specialization<T, ReadComponent>::value
                     || detail::is_specialization<T, WriteComponent>::value
                     || detail::is_specialization<T, ReadResource>::value
                     || detail::is_specialization<T, WriteResource>::value
                     || std::is_same<T, FollowedEntities>::value) {
            return;

        } else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            post_process_newly_created_entities();

        } else if constexpr (detail::is_specialization<T, AttachComponent>::value) {
            post_process_newly_attached_components<typename T::ComponentType>();

        } else if constexpr (detail::is_specialization<T, DetachComponent>::value) {
            post_process_newly_detached_components<typename T::ComponentType>();

        } else {
            static_assert(detail::unreachable<T>::value, "Invalid system data type requested!");
        }
    }

    template <typename... Ts>
    void post_process_system_data(const detail::type_tag<std::tuple<Ts...>>&) {
        (post_process_system_data_member<Ts>(), ...);
    }

    // ------------------------------------------------------------------------------------

    template <typename T> requires System<T>
    void run_system(void) {
        if (is_system_active<T>()) {
            const auto tag = detail::type_tag<typename T::SystemData>();
            typename T::SystemData data = build_system_data(system_index<T>(), tag);
            T::run(data);
            post_process_system_data(tag);
        }
    }

    template <typename... Ts>
    void run_all_systems(const TypeList<Ts...>&) {
        (run_system<Ts>(), ...);
    }

    // ------------------------------------------------------------------------------------

    template <typename T>
    auto build_system_data_member(size_t system_index) {
        if constexpr (detail::is_specialization<T, ReadComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());

        } else if constexpr (detail::is_specialization<T, WriteComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());

        } else if constexpr (detail::is_specialization<T, AttachComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>(),
                     &m_attach_component_updates[component_index<typename T::ComponentType>()]);

        } else if constexpr (detail::is_specialization<T, DetachComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>(),
                     &m_detach_component_updates[component_index<typename T::ComponentType>()]);

        } else if constexpr (std::is_same<T, FollowedEntities>::value) {
            return get_followed_entities(system_index);

        } else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            return EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_roster);

        } else if constexpr (detail::is_specialization<T, ReadResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());

        } else if constexpr (detail::is_specialization<T, WriteResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());

        } else {
            static_assert(detail::unreachable<T>::value, "Invalid system data type requested!");
        }
    }

    template<typename... Ts>
    std::tuple<Ts...> build_system_data(size_t system_index, const detail::type_tag<std::tuple<Ts...>>&) {
        return std::make_tuple<Ts...>(build_system_data_member<Ts>(system_index)...);
    }

    // ------------------------------------------------------------------------------------

    inline bool validate(void) { return m_resource_stash.validate(); }

    ThreadPool m_thread_pool;

    World(size_t nthreads) : m_active({true}), m_thread_pool(nthreads) {}

public:
    World(const World&)             = delete;
    World(World&&)                  = delete;
    World& operator=(const World&)  = delete;
    World& operator=(World&&)       = delete;
    ~World(void)                    = default;

    static constexpr size_t default_nthreads(void) {
        const int hardware = std::thread::hardware_concurrency();
        // don't want to max out peoples systems by default
        return (size_t) std::max(hardware - 2, 1);
    }

    template <typename Callable>
    static World* init( Callable&& resource_initializer
                      , size_t nthreads = default_nthreads())
    {
        World* new_world = new World(nthreads);
        resource_initializer(new_world->m_resource_stash);

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

    template <typename Callable>
    void build_entities(Callable&& f) {
        f(EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_roster));
        post_process_newly_created_entities();
    }

};

} // end namespace ark
