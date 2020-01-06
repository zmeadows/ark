#pragma once

#include "ark/component.hpp"
#include "ark/flat_entity_set.hpp"
#include "ark/type_mask.hpp"
#include "ark/third_party/ThreadPool.hpp"

#include <array>
#include <vector>
#include <unordered_map>
#include <future>

namespace ark {

class EntityRange {
    const EntityID* start_ptr;
    const EntityID* end_ptr;
public:
    const EntityID* begin(void) const { return start_ptr; }
    const EntityID* end(void) const { return end_ptr; }

    inline size_t size(void) const { return end_ptr - start_ptr; }

    EntityRange(EntityID* s, EntityID* e)
        : start_ptr(s), end_ptr(e) {}
};

class FollowedEntities {
    FlatEntitySet* m_set;
    ThreadPool* m_thread_pool;
public:
    FlatEntitySet::const_iterator begin(void) const { return m_set->cbegin(); }
    FlatEntitySet::const_iterator end(void) const { return m_set->cend(); }

    inline size_t size(void) const { return m_set->size(); }

    std::vector<EntityRange> split(size_t n) {
        std::vector<EntityRange> result;
        result.reserve(n);

        const size_t base = m_set->size() / n;
        const size_t remainder = m_set->size() % n;

        EntityID* ptr = m_set->begin_ptr();
        for (size_t i = 0; i < remainder; i++) {
            EntityID* split_begin = ptr;
            EntityID* split_end = ptr + base + 1;
            result.push_back(EntityRange(split_begin, split_end));
            ptr = split_end;
        }

        for (size_t i = remainder; i < n; i++) {
            EntityID* split_begin = ptr;
            EntityID* split_end = ptr + base;
            result.push_back(EntityRange(split_begin, split_end));
            ptr = split_end;
        }

        assert(ptr == m_set->end_ptr());

        return result;
    }

    template <typename Callable>
    inline void for_each(Callable&& f) {
        for (const EntityID id : *m_set) f(id);
    }

    template <typename Callable>
    void for_each_par(Callable&& f) {
        //TODO: precompute ranges and only updated when followed entities change...?
        const std::vector<EntityRange> ranges = split(m_thread_pool->nthreads());

        std::vector<std::future<void>> results;
        results.reserve(ranges.size());

        for (const EntityRange& r : ranges) {
            results.emplace_back(
                m_thread_pool->enqueue([&r, func = std::forward<Callable>(f)]() -> void {
                for (const EntityID id : r) {
                    func(id);
                }
            }));
        }

         for(auto&& result : results) {
             result.get();
         }
    }

    FollowedEntities(FlatEntitySet* s, ThreadPool* p) : m_set(s), m_thread_pool(p) {}
};

template <typename S>
concept bool System = requires {
    typename S::SystemData;
    typename S::Subscriptions;
} && requires(S* sys, typename S::SystemData& data, FollowedEntities entities) {
    { sys->run(entities, data) } -> void;
};

template <Component T>
class ReadComponent {
    const typename T::Storage* m_store;
public:
    using ComponentType = T;

    inline const T& operator[](EntityID id) const {
        return m_store->get(id);
    }

    ReadComponent() = delete;
    ReadComponent(const typename T::Storage* store) : m_store(store) {}
};

template <Component T>
class WriteComponent {
    typename T::Storage* m_store;
public:
    using ComponentType = T;

    inline T& operator[](EntityID id) {
        return m_store->get(id);
    }

    WriteComponent() = delete;
    WriteComponent(typename T::Storage* store) : m_store(store) {}
};

template <typename T> requires Component<T>
class DetachComponent {
    typename T::Storage* m_store;
    std::vector<EntityID>* m_world_update_queue;
public:
    using ComponentType = T;

    inline void from(EntityID id) {
        m_store->detach(id);
        m_world_update_queue->push_back(id);
    }

    DetachComponent() = delete;
    DetachComponent(typename T::Storage* store, std::vector<EntityID>* world_update_queue)
        : m_store(store), m_world_update_queue(world_update_queue) {}
};

template <typename T> requires Component<T>
class AttachComponent {
    typename T::Storage* m_store;
    std::vector<EntityID>* m_world_update_queue;

public:
    using ComponentType = T;

    template <typename... Args>
    inline void to(EntityID id, Args&&... args) {
        m_store->attach(id, std::forward<Args>(args)...);
        m_world_update_queue->push_back(id);
    }

    AttachComponent() = delete;
    AttachComponent(typename T::Storage* store, std::vector<EntityID>* world_update_queue)
        : m_store(store), m_world_update_queue(world_update_queue) {}
};

template <typename T>
class WriteResource {
    T* m_ptr;
public:
    using ResourceType = T;
    WriteResource(T* ptr) : m_ptr(ptr) {}
    inline T* operator->() { return m_ptr; }
};

template <typename T>
class ReadResource {
    const T* m_ptr;
public:
    using ResourceType = T;
    ReadResource(const T* ptr) : m_ptr(ptr) {}
    inline const T* operator->() const { return m_ptr; }
};

class EntityDestroyer {
    std::vector<EntityID>* m_world_death_row;

public:
    EntityDestroyer(std::vector<EntityID>* death_row)
        : m_world_death_row(death_row) {}

    inline void operator()(const EntityID id) {
        m_world_death_row->push_back(id);
    }
};

template <typename AllComponents>
class EntityBuilder {
    using Stash = ComponentStash<AllComponents>;
    using ComponentMask = TypeMask<AllComponents>;
    using Roster = std::unordered_map<ComponentMask, std::vector<EntityID>>;

    Stash* m_world_stash;
    Roster* m_world_roster;

public:
    EntityBuilder(Stash* stash, Roster* roster)
        : m_world_stash(stash), m_world_roster(roster) {}

    class EntitySkeleton {
        EntityID m_id;
        ComponentMask m_mask;
        Stash* m_world_stash;
        Roster* m_world_roster;

    public:
        EntitySkeleton(Stash* stash, Roster* roster)
            : m_id(next_entity_id())
            , m_mask()
            , m_world_stash(stash)
            , m_world_roster(roster) {}

        template <typename T, typename... Args> requires Component<T>
        EntitySkeleton& attach(Args&&... args) {
            typename T::Storage* storage = m_world_stash->template get<T>();
            storage->attach(m_id, std::forward<Args>(args)...);
            m_mask.set(type_list::index<T,AllComponents>());
            return *this;
        }

        EntityID id(void) const { return m_id; }

        ~EntitySkeleton(void) {
            (*m_world_roster)[m_mask].push_back(m_id);
        }
    };

    EntitySkeleton new_entity(void) {
        return EntitySkeleton(m_world_stash, m_world_roster);
    }
};

} // end namespace ark

