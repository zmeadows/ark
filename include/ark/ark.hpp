#pragma once

#include <array>
#include <bitset>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>
#include <tuple>

#include "ark/flat_entity_set.hpp"
#include "ark/flat_hash_map.hpp"
#include "ark/prelude.hpp"
#include "ark/resource.hpp"
#include "ark/storage/bucket_array.hpp"
#include "ark/system.hpp"
#include "ark/third_party/ThreadPool.hpp"

using namespace std::chrono;

namespace ark {

template <typename AllComponents, typename AllSystems, typename AllResources = TypeList<>>
class World {
    static_assert(detail::is_specialization<AllComponents, TypeList>::value);
    static_assert(detail::is_specialization<AllSystems, TypeList>::value);
    static_assert(detail::is_specialization<AllResources, TypeList>::value);

    // ------------------------------------------------------------------------------------
    // Compile-time evaluated type indices for systems, components and resources

    template <System T>
    constexpr size_t system_index(void)
    {
        return type_list::index<T, AllSystems>();
    }

    template <Component T>
    constexpr size_t component_index(void)
    {
        return type_list::index<T, AllComponents>();
    }

    template <typename T>
    constexpr size_t resource_index(void)
    {
        return type_list::index<T, AllResources>();
    }

    // ------------------------------------------------------------------------------------

    ComponentStash<AllComponents> m_component_stash;
    ResourceStash<AllResources> m_resource_stash;

    size_t m_num_entities;

    // ------------------------------------------------------------------------------------
    // Profiling

    /*
    static constexpr size_t PROFILE_FRAME_GROUP = 30;

    class TimingProfiler {
        std::array<size_t, PROFILE_FRAME_GROUP> m_recent_ns;
        size_t m_index;

        std::vector<double> m_history_ms;

    public:
        void start_timer(void);
        void stop_timer(void);

        const std::vector<double>& history_ms(void) const { return m_history_ms; }
        const double last_iteration_time_ms(void) const;
    };

    class MemoryProfiler {
        std::array<size_t, PROFILE_FRAME_GROUP> m_used_kb;
        std::array<size_t, PROFILE_FRAME_GROUP> m_allocated_kb;
        std::vector<size_t> m_history_kb;

    public:
        void profile(void);
    };

    std::unordered_map<size_t, SystemProfiler> m_system_profilers;
    std::unordered_map<size_t, ComponentProfiler> m_component_profilers;
    */

    // ------------------------------------------------------------------------------------
    // System information

    std::array<FlatEntitySet, AllSystems::size> m_followed;

    template <System T>
    inline void follow_new_entities(const std::vector<EntityID>& entities)
    {
        ARK_LOG_VERBOSE(detail::type_name<T>()
                        << " following new entities: " << entities_to_string(entities));
        m_followed[system_index<T>()].insert_new_entities(entities);
    }

    template <System T>
    inline void follow_entities(const std::vector<EntityID>& entities)
    {
        ARK_LOG_VERBOSE(detail::type_name<T>()
                        << " following entities: " << entities_to_string(entities));
        m_followed[system_index<T>()].insert_entities(entities);
    }

    template <System T>
    inline void unfollow_entities(const std::vector<EntityID>& entities)
    {
        ARK_LOG_VERBOSE(detail::type_name<T>()
                        << " unfollowing entities: " << entities_to_string(entities));
        m_followed[system_index<T>()].remove_entities(entities);
    }

    template <System T>
    inline FollowedEntities get_followed_entities(void)
    {
        return FollowedEntities(std::addressof(m_followed[system_index<T>()]), &m_thread_pool);
    }

    // ------------------------------------------------------------------------------------

    using ComponentMask = TypeMask<AllComponents>;

    // TODO: reserve space in this map at creation
    EntityMap<ComponentMask> m_entity_masks;

    template <System T>
    static constexpr ComponentMask system_mask(void)
    {
        return ComponentMask(typename T::Subscriptions());
    }

    /* ------------------------------------------------------------------------------------
                _   _ _                               _   _
      ___ _ __ | |_(_) |_ _   _    ___ _ __ ___  __ _| |_(_) ___  _ __
     / _ \ '_ \| __| | __| | | |  / __| '__/ _ \/ _` | __| |/ _ \| '_ \
    |  __/ | | | |_| | |_| |_| | | (__| | |  __/ (_| | |_| | (_) | | | |
     \___|_| |_|\__|_|\__|\__, |  \___|_|  \___|\__,_|\__|_|\___/|_| |_|
                          |___/

    */// ----------------------------------------------------------------------------------

    // Since similar entities tend to be created together, we group them by ComponentMask
    // and alert system to groups of new entities rather than each individual new entity
    std::unordered_map<ComponentMask, std::vector<EntityID>> m_new_entity_roster;

    template <System T>
    inline void alert_system_new_entities_created(const std::vector<EntityID>& new_entities,
                                                  const ComponentMask& entity_mask)
    {
        constexpr ComponentMask sys_mask = system_mask<T>();
        if (sys_mask.is_subset_of(entity_mask)) {
            follow_new_entities<T>(new_entities);
        }
    }

    template <typename... SystemTypes>
    inline void alert_all_systems_new_entities_created(const std::vector<EntityID>& new_entities,
                                                       const ComponentMask& entity_mask,
                                                       const TypeList<SystemTypes...>&)
    {
        (alert_system_new_entities_created<SystemTypes>(new_entities, entity_mask), ...);
    }

    void post_process_newly_created_entities(void)
    {
        for (auto& [mask, new_entities] : m_new_entity_roster) {
            if (!new_entities.empty()) {
                for (const EntityID id : new_entities) {
                    m_num_entities++;
                    m_entity_masks.insert(id, mask);
                }

                alert_all_systems_new_entities_created(new_entities, mask, AllSystems());
                new_entities.clear();
            }
        }

        // TODO: If this has a non-neglible performance cost,
        // use logic with total m_new_entity_roster memory usage
        // and history to decide whether or not to clear.
        m_new_entity_roster.clear();
    }

    /* ------------------------------------------------------------------------------------
                _   _ _               _           _                   _   _
      ___ _ __ | |_(_) |_ _   _    __| | ___  ___| |_ _ __ _   _  ___| |_(_) ___  _ __
     / _ \ '_ \| __| | __| | | |  / _` |/ _ \/ __| __| '__| | | |/ __| __| |/ _ \| '_ \
    |  __/ | | | |_| | |_| |_| | | (_| |  __/\__ \ |_| |  | |_| | (__| |_| | (_) | | | |
     \___|_| |_|\__|_|\__|\__, |  \__,_|\___||___/\__|_|   \__,_|\___|\__|_|\___/|_| |_|
                          |___/
    */// ----------------------------------------------------------------------------------

    std::vector<EntityID> m_death_row;

    template <System T>
    inline void alert_system_entities_destroyed(const std::vector<EntityID>& destroyed_entities,
                                                const ComponentMask& destroyed_mask)
    {
        static const ComponentMask sys_mask = system_mask<T>();
        if (sys_mask.is_subset_of(destroyed_mask)) {
            unfollow_entities<T>(destroyed_entities);
        }
    }

    template <typename... SystemTypes>
    inline void alert_all_systems_entities_destroyed(
        const std::vector<EntityID>& destroyed_entities, const ComponentMask& destroyed_mask,
        const TypeList<SystemTypes...>&)
    {
        (alert_system_entities_destroyed<SystemTypes>(destroyed_entities, destroyed_mask), ...);
    }

    template <Component T>
    void detach_component_if_exists(const EntityID id, const ComponentMask& mask)
    {
        if (mask.check(component_index<T>())) {
            typename T::Storage* store = m_component_stash.template get<T>();
            store->detach(id);
        }
    }

    template <typename... Ts>
    void _detach_all_components(const EntityID id, const ComponentMask& mask,
                                const TypeList<Ts...>&)
    {
        (detach_component_if_exists<Ts>(id, mask), ...);
    }

    void detach_all_components(const EntityID id, const ComponentMask& mask)
    {
        _detach_all_components(id, mask, AllComponents());
    }

    void post_process_destroyed_entities(void)
    {
        static std::unordered_map<ComponentMask, std::vector<EntityID>> destroyed_roster;

        if (m_death_row.empty()) return;

        ARK_LOG_VERBOSE("destroying " << m_death_row.size()
                                      << " entities: " << entities_to_string(m_death_row));

        for (const EntityID id : m_death_row) {
            m_num_entities--;
            const ComponentMask mask = m_entity_masks[id];
            m_entity_masks.remove(id);
            auto it = destroyed_roster.find(mask);
            if (it != destroyed_roster.end()) {
                it->second.push_back(id);
            }
            else {
                destroyed_roster.emplace(mask, std::vector<EntityID>(1, id));
            }
            detach_all_components(id, mask);
        }

        m_death_row.clear();

        for (auto& [mask, destroyed_entities] : destroyed_roster) {
            if (!destroyed_entities.empty()) {
                alert_all_systems_entities_destroyed(destroyed_entities, mask, AllSystems());
                destroyed_entities.clear();
            }
        }
    }

    /* ------------------------------------------------------------------------------------
         _      _             _                                                        _
      __| | ___| |_ __ _  ___| |__     ___ ___  _ __ ___  _ __   ___  _ __   ___ _ __ | |_ ___
     / _` |/ _ \ __/ _` |/ __| '_ \   / __/ _ \| '_ ` _ \| '_ \ / _ \| '_ \ / _ \ '_ \| __/ __|
    | (_| |  __/ || (_| | (__| | | | | (_| (_) | | | | | | |_) | (_) | | | |  __/ | | | |_\__ \
     \__,_|\___|\__\__,_|\___|_| |_|  \___\___/|_| |_| |_| .__/ \___/|_| |_|\___|_| |_|\__|___/
                                                         |_|

    */// ----------------------------------------------------------------------------------

    // OPTIMIZE: use llvm::SmallVector-style stack-preferred vector here as well as for
    // m_attach_component_updates
    std::array<std::vector<EntityID>, AllComponents::size> m_detach_component_updates;

    template <System SystemType, Component ComponentType>
    void alert_system_component_detached_from_entities(const std::vector<EntityID>& entities)
    {
        if constexpr (system_mask<SystemType>().check(component_index<ComponentType>())) {
            unfollow_entities<SystemType>(entities);
        }
    }

    template <Component ComponentType, typename... SystemTypes>
    inline void _alert_all_systems_component_detached_from_entities(
        const std::vector<EntityID>& entities, const TypeList<SystemTypes...>&)
    {
        (alert_system_component_detached_from_entities<ComponentType, SystemTypes>(entities), ...);
    }

    template <Component T>
    inline void alert_all_systems_component_detached_from_entities(
        const std::vector<EntityID>& entities)
    {
        _alert_all_systems_component_detached_from_entities<T>(entities, AllSystems());
    }

    template <Component T>
    void post_process_newly_detached_component(const std::vector<EntityID>& entities)
    {
        for (const EntityID id : entities) {
            ComponentMask& entity_mask = m_entity_masks[id];
            entity_mask.unset(component_index<T>());
        }
        alert_all_systems_component_detached_from_entities<T>(entities);
    }

    template <Component T>
    void post_process_newly_detached_components(void)
    {
        std::vector<EntityID>& updates = m_detach_component_updates[component_index<T>()];
        post_process_newly_detached_component<T>(updates);
        updates.clear();
    }

    /* ------------------------------------------------------------------------------------
           _   _             _                                                        _
      __ _| |_| |_ __ _  ___| |__     ___ ___  _ __ ___  _ __   ___  _ __   ___ _ __ | |_ ___
     / _` | __| __/ _` |/ __| '_ \   / __/ _ \| '_ ` _ \| '_ \ / _ \| '_ \ / _ \ '_ \| __/ __|
    | (_| | |_| || (_| | (__| | | | | (_| (_) | | | | | | |_) | (_) | | | |  __/ | | | |_\__ \
     \__,_|\__|\__\__,_|\___|_| |_|  \___\___/|_| |_| |_| .__/ \___/|_| |_|\___|_| |_|\__|___/
                                                        |_|

    */// ----------------------------------------------------------------------------------

    std::array<std::vector<EntityID>, AllComponents::size> m_attach_component_updates;

    template <Component ComponentType, System SystemType>
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
    inline void _alert_all_systems_component_attached_to_entities(
        const std::vector<EntityID>& entities, const TypeList<SystemTypes...>&)
    {
        (alert_system_component_attached_to_entities<ComponentType, SystemTypes>(entities), ...);
    }

    template <Component T>
    inline void alert_all_systems_component_attached_to_entities(
        const std::vector<EntityID>& entities)
    {
        _alert_all_systems_component_attached_to_entities<T>(entities, AllSystems());
    }

    template <Component T>
    void post_process_newly_attached_component(const std::vector<EntityID>& entities)
    {
        for (const EntityID id : entities) {
            ComponentMask& entity_mask = m_entity_masks[id];
            entity_mask.set(component_index<T>());
        }
        alert_all_systems_component_attached_to_entities<T>(entities);
    }

    template <Component T>
    void post_process_newly_attached_components(void)
    {
        std::vector<EntityID>& updates = m_attach_component_updates[component_index<T>()];
        post_process_newly_attached_component<T>(updates);
        updates.clear();
    }

    // ------------------------------------------------------------------------------------

    template <typename T>
    void post_process_system_data_member(void)
    {
        if constexpr (detail::is_specialization<T, ReadComponent>::value ||
                      detail::is_specialization<T, WriteComponent>::value ||
                      detail::is_specialization<T, ReadResource>::value ||
                      detail::is_specialization<T, WriteResource>::value) {
            return;
        }
        else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            post_process_newly_created_entities();
        }
        else if constexpr (std::is_same<T, EntityDestroyer>::value) {
            post_process_destroyed_entities();
        }
        else if constexpr (detail::is_specialization<T, AttachComponent>::value) {
            post_process_newly_attached_components<typename T::ComponentType>();
        }
        else if constexpr (detail::is_specialization<T, DetachComponent>::value) {
            post_process_newly_detached_components<typename T::ComponentType>();
        }
        else {
            static_assert(detail::unreachable<T>::value,
                          "interal ark error: Invalid system data type requested in "
                          "post_process_system_data_member.");
        }
    }

    // template <Component T>
    // float defragment_component_storage(double time_left) {
    //     if constexpr(storage::is_bucket_array<typename T::Storage>::value) {
    //         typename T::Storage* store = m_component_stash.template get<T>();
    //         const std::optional<double> predicted_time = store->estimate_maintenance_time();
    //         if (predicted_time && *predicted_time < time_left) {
    //             const auto start = high_resolution_clock::now();
    //             store->maintenance();
    //             const auto end = high_resolution_clock::now();
    //             const double dur = duration_cast<duration<double>>(end - start).count();
    //             return time_left - dur;
    //         } else {
    //             return time_left;
    //         }
    //     } else {
    //         return time_left;
    //     }
    // }

    // void run_maintanence(float allowed_time) {
    //     run_maintanence(allowed_time, AllComponents());
    // }

    // template <typename T, typename... Ts>
    // void run_maintanence(float allowed_time, const TypeList<T,Ts...>&) {
    //     allowed_time = defragment_component_storage<T>(allowed_time);
    //     run_maintanence(allowed_time, TypeList<Ts...>());
    // }

    // void run_maintanence(float, const TypeList<>&) {
    //     return;
    // }

    template <typename... Ts>
    inline void post_process_system_data(const detail::type_tag<std::tuple<Ts...>>&)
    {
        (post_process_system_data_member<Ts>(), ...);
    }

    template <System T>
    inline void post_process_system_data(void)
    {
        static const auto tag = detail::type_tag<typename T::SystemData>();
        post_process_system_data(tag);
    }

    // ------------------------------------------------------------------------------------

    template <System T>
    void run_system(void)
    {
        static const auto tag = detail::type_tag<typename T::SystemData>();
        typename T::SystemData data = build_system_data(tag);
        T::run(get_followed_entities<T>(), data);
    }

    template <System T>
    void run_system_and_postprocess(void)
    {
        run_system<T>();
        post_process_system_data<T>();
    }

    // ------------------------------------------------------------------------------------

    template <typename T>
    inline auto build_system_data_member(void)
    {
        if constexpr (detail::is_specialization<T, ReadComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());
        }
        else if constexpr (detail::is_specialization<T, WriteComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>());
        }
        else if constexpr (detail::is_specialization<T, AttachComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>(),
                     &m_attach_component_updates[component_index<typename T::ComponentType>()]);
        }
        else if constexpr (detail::is_specialization<T, DetachComponent>::value) {
            return T(m_component_stash.template get<typename T::ComponentType>(),
                     &m_detach_component_updates[component_index<typename T::ComponentType>()]);
        }
        else if constexpr (std::is_same<T, EntityBuilder<AllComponents>>::value) {
            return EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_roster);
        }
        else if constexpr (std::is_same<T, EntityDestroyer>::value) {
            return EntityDestroyer(&m_death_row);
        }
        else if constexpr (detail::is_specialization<T, ReadResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());
        }
        else if constexpr (detail::is_specialization<T, WriteResource>::value) {
            return T(m_resource_stash.template get<typename T::ResourceType>());
        }
        else {
            static_assert(detail::unreachable<T>::value, "Invalid system data type requested!");
        }
    }

    template <typename... Ts>
    inline std::tuple<Ts...> build_system_data(const detail::type_tag<std::tuple<Ts...>>&)
    {
        return std::make_tuple<Ts...>(build_system_data_member<Ts>()...);
    }

    // ------------------------------------------------------------------------------------

    inline bool validate(void) { return m_resource_stash.all_initialized(); }

    ThreadPool m_thread_pool;

    template <System T>
    inline void start_system_in_parallel(std::vector<std::future<void>>& results)
    {
        results.emplace_back(m_thread_pool.enqueue([this]() { this->run_system<T>(); }));
    }

public:
    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;
    ~World(void) = default;

    static constexpr size_t default_nthreads(void)
    {
        const int hardware = std::thread::hardware_concurrency();
        // don't want to max out by default, to avoid locking the system
        return (size_t)std::max(hardware - 2, 1);
    }

    World(size_t nthreads = default_nthreads()) : m_num_entities(0), m_thread_pool(nthreads) {}

    template <typename Callable>
    static World* init(Callable&& resource_initializer, size_t nthreads = default_nthreads())
    {
        World* new_world = new World(nthreads);
        resource_initializer(new_world->m_resource_stash);

        if (new_world->validate()) {
            return new_world;
        }
        else {
            return nullptr;
        }
    }

    template <typename... Ts>
    void run_systems_sequential()
    {
        static_assert(sizeof...(Ts) > 0, "Must pass >= 1 system type to run_systems_sequential.");
        (run_system_and_postprocess<Ts>(), ...);
    }

    template <typename... Ts>
    void run_systems_parallel()
    {
        static_assert(sizeof...(Ts) > 0, "Must pass >= 1 system type to run_systems_parallel.");
        // TODO: Verify Systems can be run in parallel
        std::vector<std::future<void>> results;
        (start_system_in_parallel<Ts>(results), ...);
        for (auto&& result : results) {
            result.get();
        }
        (post_process_system_data<Ts>(), ...);
    }

    template <typename T>
    inline T* get_resource(void)
    {
        return m_resource_stash.template get<T>();
    }

    template <typename Callable>
    inline void build_entities(Callable&& f)
    {
        f(EntityBuilder<AllComponents>(&m_component_stash, &m_new_entity_roster));
        post_process_newly_created_entities();
    }

    // void post_frame_upkeep(double allotted_time) {
    //     run_maintanence(allotted_time, AllComponents());
    // }

    inline size_t entity_count(void) const { return m_num_entities; }
};

}  // end namespace ark
