#pragma once

#include "ark/third_party/skarupke/bytell_hash_map.hpp"

#include <assert.h>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace ark {

using EntityID = uint32_t;

EntityID next_entity_id(void) {
    static std::atomic<EntityID> next_id = 0;
    return next_id.fetch_add(1);
}

class EntitySet {
    ska::bytell_hash_set<EntityID> m_set;
public:
    inline void insert(EntityID id) { m_set.insert(id); }
    inline void remove(EntityID id) { m_set.erase(id); }
    inline bool contains(EntityID id) { return m_set.find(id) != m_set.end(); }

    using const_iterator = decltype(m_set)::const_iterator;
    const_iterator begin(void) const { return m_set.cbegin(); }
    const_iterator end(void) const { return m_set.cend(); }
};

class EntityBuffer {
    std::vector<EntityID> m_buffer;
public:
    inline void insert(EntityID id) {
        assert(std::find(m_buffer.begin(), m_buffer.end(), id) == m_buffer.end());
        m_buffer.push_back(id);
    }

    inline void clear(void) { m_buffer.clear(); }

    using const_iterator = decltype(m_buffer)::const_iterator;
    const_iterator begin(void) const { return m_buffer.cbegin(); }
    const_iterator end(void) const { return m_buffer.cend(); }
};

// a const view into an EntitySet for iterating over inside Systems
class FollowedEntities {
    const EntitySet* m_set;
public:
    EntitySet::const_iterator begin(void) const { return m_set->begin(); }
    EntitySet::const_iterator end(void) const { return m_set->end(); }

    FollowedEntities(const EntitySet* s) : m_set(s) {}
};

template <size_t N>
class BitMask {
    std::bitset<N> m_bitset;
public:
    inline void set(size_t bit) { m_bitset.set(bit); }
    inline void unset(size_t bit) { m_bitset.reset(bit); }
    inline void is_subset_of(const BitMask<N>& other) {
        return (*this & other == *this);
    }
};

template<typename... Ts>
struct TypeList {
    static constexpr std::size_t size{ sizeof... (Ts) };
};

namespace detail {

template <class T>
constexpr std::string_view type_name()
{
    using namespace std;
#ifdef __clang__
    string_view p = __PRETTY_FUNCTION__;
    return string_view(p.data() + 34, p.size() - 34 - 1);
#elif defined(__GNUC__)
    string_view p = __PRETTY_FUNCTION__;
#  if __cplusplus < 201402
    return string_view(p.data() + 36, p.size() - 36 - 1);
#  else
    return string_view(p.data() + 49, p.find(';', 49) - 49);
#  endif
#elif defined(_MSC_VER)
    string_view p = __FUNCSIG__;
    return string_view(p.data() + 84, p.size() - 84 - 7);
#endif
}

template<typename T> struct type_tag {};

template <typename... T>
struct unreachable { static constexpr bool value = false; };

template< class T, class U >
concept bool SameHelper = std::is_same_v<T, U>;

template< class T, class U >
concept bool Same = detail::SameHelper<T, U> && detail::SameHelper<U, T>;

template<typename>
constexpr std::size_t locate(std::size_t) {
    return static_cast<std::size_t>(-1);
}

template<typename IndexedType, typename T, typename... Ts>
constexpr std::size_t locate(std::size_t ind = 0) {
    if (std::is_same<IndexedType, T>::value) {
        return ind;
    } else {
        return locate<IndexedType, Ts...>(ind + 1);
    }
}


template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

template<typename T, typename>
struct IndexOf;

template<typename T, typename... ListedTypes>
struct IndexOf<T, TypeList<ListedTypes...>>
  : std::integral_constant<std::size_t, locate<T, ListedTypes...>()>
{};

template <typename T, typename List>
static constexpr size_t index_in_type_list(void) {
    static_assert(detail::is_specialization<List, TypeList>::value);
    constexpr size_t idx = detail::IndexOf<T, List>::value;
    static_assert(idx < List::size);
    return idx;
}

} // end namespace ark::detail

template <typename S>
concept bool ComponentStorage = requires(S* store, const S* const_store, EntityID id) {
    typename S::ComponentType;
    { store->get(id)    } -> typename S::ComponentType&;
    { const_store->get(id)    } -> const typename S::ComponentType&;
    { store->get_if(id) } -> typename S::ComponentType*;
    { const_store->has(id)    } -> bool;
    { store->attach(id) } -> typename S::ComponentType&;
    { store->detach(id) } -> void;
};

template <typename C>
concept bool Component = ComponentStorage<typename C::Storage>
                         && detail::Same<typename C::Storage::ComponentType, C>;

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
class RemoveComponent {
    const typename T::Storage* m_store;
    EntityBuffer m_buffer;
public:
    using ComponentType = T;

    inline void operator()(EntityID id) {
        m_buffer.insert(id);
        m_store->detach(id);
    }

    template <typename Callable>
    inline void post_process(Callable&& f) {
        f(m_buffer);
        m_buffer.clear();
    }

    RemoveComponent() = delete;
    RemoveComponent(const typename T::Storage* store) : m_store(store) {}
};

template <Component T>
class AttachComponent {
    typename T::Storage* m_store;
    EntityBuffer m_buffer;

    template <typename A, typename B, typename C>
    friend class World;

    template <typename Callable>
    inline void post_process(Callable&& f) { f(std::move(m_buffer)); }

public:
    using ComponentType = T;

    template <typename... Args>
    inline void operator()(EntityID id, Args&&... args) {
        m_buffer.insert(id);
        m_store->attach(id, std::forward<Args>(args)...);
    }

    AttachComponent() = delete;
    AttachComponent(typename T::Storage* store) : m_store(store) {}
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

template <typename S>
concept bool System = requires {
    typename S::SystemData;
} && requires(typename S::SystemData& data) {
    { S::run(data) } -> void;
};

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

    ComponentStash(const ComponentStash&) = delete;
    ComponentStash(ComponentStash&&) = delete;
    ComponentStash& operator=(const ComponentStash&) = delete;
    ComponentStash& operator=(ComponentStash&&) = delete;

    ~ComponentStash(void) {
        cleanup_component_storage(AllComponents());
    }
};

template <typename AllSystems>
class SystemStash {
    std::array<void*, AllSystems::size> m_data;
    std::array<bool, AllSystems::size> m_active;
    std::array<EntitySet, AllSystems::size> m_followed;

    template <typename T> requires System<T>
    static constexpr size_t system_index(void) {
        return detail::index_in_type_list<T, AllSystems>();
    }

    template <typename T> requires System<T>
    void cleanup(void) { delete get_system_data<T>(); }

    template <typename... Ts>
    void cleanup_system_data(const TypeList<Ts...>&) {
        (cleanup<Ts>(), ...);
    }

public:
    template <typename T> requires System<T>
    inline typename T::SystemData* get_system_data(void) {
        return static_cast<typename T::SystemData*>(m_data[system_index<T>()]);
    }

    template <typename T> requires System<T>
    inline void set_system_data(typename T::SystemData* data) {
        m_data[system_index<T>()] = static_cast<void*>(data);
    }

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

    SystemStash(void) : m_data({nullptr}) {}

    SystemStash(const SystemStash&) = delete;
    SystemStash(SystemStash&&) = delete;
    SystemStash& operator=(const SystemStash&) = delete;
    SystemStash& operator=(SystemStash&&) = delete;

    ~SystemStash(void) {
        cleanup_system_data(AllSystems());
    }

};

template <typename AllResources>
class ResourceStash {
    std::array<void*, AllResources::size> m_storage;

    template <typename T>
    static constexpr size_t resource_index(void) {
        return detail::index_in_type_list<T, AllResources>();
    }

    template <typename T>
    void cleanup(void) { delete get<T>(); }

    template <typename... Ts>
    void cleanup_resource_storage(const TypeList<Ts...>&) {
        (cleanup<Ts>(), ...);
    }

public:
    template <typename T, typename... Args>
    void construct(Args&&... args) {
        assert(m_storage[resource_index<T>()] == nullptr
               && "attempted to construct resource that has already been constructed.");
        T* new_resource_ptr = new T(std::forward<Args>(args)...);
        assert(new_resource_ptr && "Failed to construct resource");
        m_storage[resource_index<T>()] = static_cast<void*>(new_resource_ptr);
    }

    template <typename T>
    inline T* get(void) {
        return static_cast<T*>(m_storage[resource_index<T>()]);
    }

    template <typename T>
    inline const typename T::Storage* get(void) const {
        return static_cast<const T*>(m_storage[resource_index<T>()]);
    }

    bool validate(void) {
        for (const void* ptr : m_storage) {
            if (ptr == nullptr) {
                return false;
            }
        }
        return true;
    }

    ResourceStash(void) : m_storage({nullptr}) {}

    ResourceStash(const ResourceStash&) = delete;
    ResourceStash(ResourceStash&&) = delete;
    ResourceStash& operator=(const ResourceStash&) = delete;
    ResourceStash& operator=(ResourceStash&&) = delete;

    ~ResourceStash(void) {
        cleanup_resource_storage(AllResources());
    }
};

template <typename AllComponents>
class EntityBuilder {

    using Stash = ComponentStash<AllComponents>;
    Stash* m_stash;

    struct Spec {
        EntityID id;
        BitMask<AllComponents::size> mask;
    };

    std::vector<Spec> m_specs;

    template <typename A, typename B, typename C>
    friend class World;

    template <typename Callable>
    void world_post_process(Callable&& f) {
        f(m_specs);
        m_specs.clear();
    }

public:
    EntityBuilder(Stash* stash) : m_stash(stash) {}

    class EntitySkeleton {
        Stash* m_stash;
        Spec* m_spec;

    public:
        EntitySkeleton(Stash* stash, Spec* spec) : m_stash(stash), m_spec(spec) {}

        template <typename T, typename... Args> requires Component<T>
        EntitySkeleton& attach(Args&&... args) {
            typename T::Storage* storage = m_stash->template get<T>();
            storage->attach(m_spec->id, std::forward<Args>(args)...);
            m_spec->mask.set(m_stash->template component_index<T>());
            return *this;
        }
    };

    EntitySkeleton new_entity(void) {
        m_specs.emplace_back(Spec {
            .id = next_entity_id(),
            .mask = BitMask<AllComponents::size>()
        });

        return EntitySkeleton(m_stash, std::addressof(m_specs.back()));
    }


};




}
