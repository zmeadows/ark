#pragma once

namespace ark {

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

    // TODO: rename for clarity
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

} // end namespace ark

