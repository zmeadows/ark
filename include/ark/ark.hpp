#pragma once

#include "ark/prelude.hpp"
#include "ark/system.hpp"
#include "ark/resource.hpp"
#include "ark/flat_entity_set.hpp"
#include "ark/flat_hash_map.hpp"

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

    ComponentStash<AllComponents> m_component_stash;
    ResourceStash<AllResources> m_resource_stash;
    SystemStash<AllSystems> m_system_stash;

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

    std::vector<EntitySpec<AllComponents>> m_new_entity_updates;

    template <typename T> requires System<T>
    void alert_system_new_entity_created(EntityID id, const ComponentMask& entity_mask) {
        static const ComponentMask sys_mask = system_mask<T>();
        if (sys_mask.is_subset_of(entity_mask)) {
            m_system_stash.template follow_entity<T>(id);
        }
    }

    template <typename... Ts>
    void alert_all_systems_new_entity_created( EntityID id
                                             , const ComponentMask& entity_mask
                                             , const TypeList<Ts...>&)
    {
        (alert_system_new_entity_created<Ts>(id, entity_mask), ...);
    }

    void post_process_newly_created_entities(void) {
        for (const auto& spec : m_new_entity_updates) {
            assert(!m_entity_masks.lookup(spec.id)
                   && "Unexpected entity found to already exist during system post-process phase");

            alert_all_systems_new_entity_created(spec.id, spec.mask, AllSystems());
            m_entity_masks.insert(spec.id, spec.mask);
        }
        m_new_entity_updates.clear();
    }

    // ------------------------------------------------------------------------------------

    std::array<FlatEntitySet, AllComponents::size> m_detach_component_updates;

    template <typename ComponentType, typename SystemType>
    requires System<SystemType> && Component<ComponentType>
    void alert_system_component_detached_from_entity(EntityID id) {
        if constexpr (system_mask<SystemType>().check(component_index<ComponentType>())) {
            m_system_stash.template unfollow_entity<SystemType>(id);
        }
    }

    template <typename ComponentType, typename... SystemTypes>
    requires Component<ComponentType>
    inline void _alert_all_systems_component_detached_from_entity(EntityID id,
                                                                  const TypeList<SystemTypes...>&)
    {
        (alert_system_component_detached_from_entity<ComponentType, SystemTypes>(id), ...);
    }

    template <typename T> requires Component<T>
    inline void alert_all_systems_component_detached_from_entity(EntityID id)
    {
        _alert_all_systems_component_detached_from_entity<T>(id, AllSystems());
    }

    template <typename T> requires Component<T>
    void post_process_newly_detached_component(EntityID id) {
        ComponentMask& entity_mask = m_entity_masks[id];
        entity_mask.unset(component_index<T>());
        alert_all_systems_component_detached_from_entity<T>(id);
    }

    template <typename T> requires Component<T>
    void post_process_newly_detached_components(void) {
        for (const EntityID id : m_detach_component_updates[component_index<T>()]) {
            post_process_newly_detached_component<T>(id);
        }
        m_detach_component_updates.clear();
    }

    // ------------------------------------------------------------------------------------

    std::array<FlatEntitySet, AllComponents::size> m_attach_component_updates;

    template <typename ComponentType, typename SystemType>
    requires System<SystemType> && Component<ComponentType>
    void alert_system_component_attached_to_entity(EntityID id) {
        if constexpr (system_mask<SystemType>().check(component_index<ComponentType>())) {
            const ComponentMask& entity_mask = m_entity_masks[id];
            if (system_mask<SystemType>().is_subset_of(entity_mask)) {
                m_system_stash.template follow_entity<SystemType>(id);
            }
        }
    }

    template <typename ComponentType, typename... SystemTypes>
    inline void _alert_all_systems_component_attached_to_entity(EntityID id, const TypeList<SystemTypes...>&) {
        (alert_system_component_attached_to_entity<ComponentType, SystemTypes>(id), ...);
    }

    template <typename T> requires Component<T>
    inline void alert_all_systems_component_attached_to_entity(EntityID id)
    {
        _alert_all_systems_component_attached_to_entity<T>(id, AllSystems());
    }

    template <typename T> requires Component<T>
    void post_process_newly_attached_component(EntityID id) {
        ComponentMask& entity_mask = m_entity_masks[id];
        entity_mask.set(component_index<T>());
        alert_all_systems_component_attached_to_entity<T>(id);
    }

    template <typename T> requires Component<T>
    void post_process_newly_attached_components(void) {
        for (const EntityID id : m_attach_component_updates[component_index<T>()]) {
            post_process_newly_attached_component<T>(id);
        }

        m_attach_component_updates.clear();
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
        if (m_system_stash.template is_active<T>()) {
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
            return FollowedEntities(m_system_stash.get_followed_entities(system_index));

        } else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            return EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_updates);

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

    World() = default;

public:
    World(const World&)             = delete;
    World(World&&)                  = delete;
    World& operator=(const World&)  = delete;
    World& operator=(World&&)       = delete;
    ~World(void)                    = default;

    template <typename Callable>
        //TODO: return unique_ptr
    static World* init(Callable&& resource_initializer) {
        World* new_world = new World();
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
        f(EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_updates));
        post_process_newly_created_entities();
    }

};

} // end namespace ark
