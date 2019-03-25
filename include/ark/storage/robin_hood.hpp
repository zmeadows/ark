#pragma once

#include "ark/prelude.hpp"
#include "ark/flat_hash_map.hpp"

namespace ark {

template <typename T>
class RobinHoodStorage {
    EntityMap<T> m_map;
public:
    using ComponentType = T;

    inline T& get(EntityID id) {
        T* val = m_map.lookup(id);
        ARK_ASSERT(val, "RobinHoodStorage: Entity lookup failed for " << detail::type_name<T>() << " with entity: " << id);
        return *val;
    }

    inline const T& get(EntityID id) const {
        const T* val = m_map.lookup(id);
        ARK_ASSERT(val, "RobinHoodStorage: Entity lookup failed for " << detail::type_name<T>() << " with entity: " << id);
        return *val;
    }

    inline T* get_if(EntityID id) {
        return m_map.lookup(id);
    }

    inline bool has(EntityID id) const {
        return m_map.lookup(id) != nullptr;
    }

    template <typename... Args>
    inline T& attach(EntityID id, Args&&... args) {
        T* val = m_map.insert(id, std::forward<Args>(args)...);
        return *val;
    }

    inline void detach(EntityID id) {
        m_map.remove(id);
    }

};

}
