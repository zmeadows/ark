#pragma once

#include "ark/component.hpp"
#include "ark/flat_entity_set.hpp"
#include "ark/type_mask.hpp"

#include <array>
#include <vector>

namespace ark {

template <typename S>
concept bool System = requires {
    typename S::SystemData;
    typename S::Subscriptions;
} && requires(typename S::SystemData& data) {
    { S::run(data) } -> void;
};

template <typename AllSystems>
class SystemStash {
    std::array<bool, AllSystems::size> m_active;
    std::array<FlatEntitySet, AllSystems::size> m_followed;

    template <typename T> requires System<T>
    static constexpr size_t system_index(void) {
        return detail::index_in_type_list<T, AllSystems>();
    }

public:
    template <typename T> requires System<T>
    inline bool is_active(void) { return m_active[system_index<T>()]; }

    template <typename T> requires System<T>
    inline void set_inactive(void) { m_active[system_index<T>()] = false; }

    template <typename T> requires System<T>
    inline void set_active(void) { m_active[system_index<T>()] = true; }

    template <typename T> requires System<T>
    inline void toggle_active(void) { m_active[system_index<T>()] = !m_active[system_index<T>()]; }

    template <typename T> requires System<T>
    inline void follow_entity(EntityID id) {
        assert(!m_followed[system_index<T>()].contains(id));
        m_followed[system_index<T>()].insert(id);
    }

    template <typename T> requires System<T>
    inline void unfollow_entity(EntityID id) {
        assert(m_followed[system_index<T>()].contains(id));
        m_followed[system_index<T>()].remove(id);
    }

    template <typename T> requires System<T>
    inline FollowedEntities get_followed_entities(void) {
        return Followed(std::addressof(m_followed[system_index<T>()]));
    }

    inline FollowedEntities get_followed_entities(const size_t sys_idx) {
        return FollowedEntities(std::addressof(m_followed[sys_idx]));
    }

    SystemStash(void) : m_active({true}) {}

    SystemStash(const SystemStash&)            = delete;
    SystemStash(SystemStash&&)                 = delete;
    SystemStash& operator=(const SystemStash&) = delete;
    SystemStash& operator=(SystemStash&&)      = delete;

    ~SystemStash(void) = default;
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
    FlatEntitySet* m_world_update_list;
public:
    using ComponentType = T;

    inline void operator()(EntityID id) {
        m_store->detach(id);
        m_world_update_list->insert(id);
    }

    DetachComponent() = delete;
    DetachComponent(typename T::Storage* store, FlatEntitySet* world_update_list)
        : m_store(store), m_world_update_list(world_update_list) {}
};

template <typename T> requires Component<T>
class AttachComponent {
    typename T::Storage* m_store;
    FlatEntitySet* m_world_update_list;

public:
    using ComponentType = T;

    template <typename... Args>
    inline void operator()(EntityID id, Args&&... args) {
        m_store->attach(id, std::forward<Args>(args)...);
        m_world_update_list->insert(id);
    }

    AttachComponent() = delete;
    AttachComponent(typename T::Storage* store, FlatEntitySet* world_update_list)
        : m_store(store), m_world_update_list(world_update_list) {}
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


template <typename AllComponents>
class EntityBuilder {
    using Stash = ComponentStash<AllComponents>;
    using Spec = EntitySpec<AllComponents>;

    Stash* m_stash;
    std::vector<Spec>* m_specs;

    template <typename T> requires Component<T>
    static constexpr size_t component_index(void) {
        return detail::index_in_type_list<T, AllComponents>();
    }

public:
    EntityBuilder(Stash* stash, std::vector<Spec>* specs)
        : m_stash(stash), m_specs(specs) {}

    class EntitySkeleton {
        Stash* m_stash;
        Spec* m_spec;

    public:
        EntitySkeleton(Stash* stash, Spec* spec) : m_stash(stash), m_spec(spec) {}

        template <typename T, typename... Args> requires Component<T>
        EntitySkeleton& attach(Args&&... args) {
            typename T::Storage* storage = m_stash->template get<T>();
            storage->attach(m_spec->id, std::forward<Args>(args)...);
            m_spec->mask.set(component_index<T>());
            return *this;
        }
    };

    EntitySkeleton new_entity(void) {
        m_specs->emplace_back(Spec {
            .id = next_entity_id(),
            .mask = TypeMask<AllComponents>()
        });

        return EntitySkeleton(m_stash, std::addressof(m_specs->back()));
    }
};

} // end namespace ark

