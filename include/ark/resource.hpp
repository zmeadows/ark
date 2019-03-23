#pragma once

#include "ark/type_list.hpp"

#include <vector>

namespace ark {

template <typename AllResources>
class ResourceStash {
    std::vector<void*> m_storage;
    std::vector<bool> m_owned;

    template <typename... Ts>
    void cleanup_all_resources(const TypeList<Ts...>&) {
        (cleanup<Ts>(), ...);
    }

public:
    template <typename T, typename... Args>
    void construct_and_own(Args&&... args) {
        const size_t resource_index = type_list::index<T,AllResources>();

        ARK_ASSERT(m_storage[resource_index] == nullptr,
                   "attempted to double-construct resource with type: " << detail::type_name<T>());

        T* new_resource_ptr = new T(std::forward<Args>(args)...);
        ARK_ASSERT(new_resource_ptr,
                   "failed to construct resource with type: " << detail::type_name<T>());
        m_storage[resource_index] = static_cast<void*>(new_resource_ptr);
        m_owned[resource_index] = true;
    }

    template <typename T>
    void store_unowned(T* ptr) {
        ARK_ASSERT(ptr, "attempted to store nullptr in ResourceStash for type: " << detail::type_name<T>());
        m_storage[type_list::index<T,AllResources>()] = static_cast<void*>(ptr);
        m_owned[type_list::index<T,AllResources>()] = false;
    }

    template <typename T>
    inline T* get(void) {
        return static_cast<T*>(m_storage[type_list::index<T,AllResources>()]);
    }

    bool all_initialized(void) {
        for (const void* ptr : m_storage) {
            if (ptr == nullptr) {
                return false;
            }
        }
        return true;
    }

    template <typename T>
    void cleanup(void) {
        if (m_owned[type_list::index<T, AllResources>()]) {
            delete get<T>();
        }
        m_storage[type_list::index<T,AllResources>()] = nullptr;
    }

    ResourceStash(void)
        : m_storage(AllResources::size, nullptr)
        , m_owned(AllResources::size, false)
    {}

    ResourceStash(const ResourceStash&) = delete;
    ResourceStash(ResourceStash&&) = delete;
    ResourceStash& operator=(const ResourceStash&) = delete;
    ResourceStash& operator=(ResourceStash&&) = delete;

    ~ResourceStash(void) {
        cleanup_all_resources(AllResources());
    }
};

} // end namespace ark

