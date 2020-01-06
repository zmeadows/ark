#pragma once

#include "ark/prelude.hpp"

#include <algorithm>
#include <vector>

namespace ark {

namespace detail {

template <typename T>
inline constexpr bool is_power_of_two(T n) {
    return (n & (n - 1)) == 0;
}

constexpr uint32_t hash_id(EntityID id) { return 3*id; }

} // end namespace ark::detail

// An open addressing hash table using robin hood hashing
// for mapping EntityIDs to *small types* like storage handles.
template <typename V>
class EntityMap {
    EntityID* m_keys;
    V* m_values;
    size_t m_count;
    size_t m_capacity;
    size_t m_longest_probe;
    double m_max_load_factor;
    static const EntityID EMPTY_SENTINEL = 0;
    static const EntityID TOMBSTONE_SENTINEL = 1;

public:
    EntityMap(size_t initial_capacity)
        : m_keys( (EntityID*) malloc(sizeof(EntityID) * initial_capacity) )
        , m_values( (V*) malloc(sizeof(V) * initial_capacity) )
        , m_count(0)
        , m_capacity(initial_capacity)
        , m_longest_probe(0)
        , m_max_load_factor(0.5)
    {
        ARK_ASSERT(initial_capacity > 0, "initial capacity must be greater than 0.");
        ARK_ASSERT(detail::is_power_of_two(initial_capacity), "capacity must always be a power of two!");
        ARK_ASSERT(m_keys, "Failed to initialize memory for EntityMap keys");
        ARK_ASSERT(m_values, "Failed to initialize memory for EntityMap values");

        for (size_t i = 0; i < m_capacity; i++) {
            m_keys[i] = EMPTY_SENTINEL;
        }
    }

    EntityMap() : EntityMap(64) {}

    EntityMap& operator=(const EntityMap&) = delete;
    EntityMap& operator=(EntityMap&&)      = delete;
    EntityMap(EntityMap&&)      = delete;
    EntityMap(const EntityMap&) = delete;

    ~EntityMap(void) {
        for (size_t i = 0; i < m_capacity; i++) {
            const EntityID id = m_keys[i];
            if (id != EMPTY_SENTINEL && id != TOMBSTONE_SENTINEL) {
                m_values[i].~V();
            }
        }

        free(m_keys);
        free(m_values);
        m_keys = nullptr;
        m_values = nullptr;
        m_count = 0;
        m_capacity = 0;
        m_longest_probe = 0;
    }

    inline size_t size(void) const { return m_count; }

    inline double load_factor() const {
        return static_cast<double>(m_count) / static_cast<double>(m_capacity);
    }

    V* lookup(EntityID lookup_id) const {
        const uint64_t N = m_capacity - 1;
        uint32_t probe_index = detail::hash_id(lookup_id) & N;
        uint64_t distance_from_initial_bucket = 0;

        while (true) {
            const EntityID probed_id = m_keys[probe_index];

            if (probed_id == lookup_id) {
                return m_values + probe_index;
            } else if (probed_id == EMPTY_SENTINEL) {
                return nullptr;
            }

            probe_index = (probe_index + 1) & N;
            distance_from_initial_bucket++;

            if (distance_from_initial_bucket > m_longest_probe) {
                return nullptr;
            }
        }
    }

    template <typename... Args>
    V* insert(EntityID new_id, Args&&... args) {
        if (load_factor() > m_max_load_factor) {
            rehash(m_capacity * 2);
        }

        const uint64_t N = m_capacity - 1;
        uint32_t probe_index = detail::hash_id(new_id) & N;

        uint64_t dib = 0; // 'd'istance from 'i'nitial 'b'ucket

        V new_value(std::forward<Args>(args)...);

        while (true) {
            EntityID& probed_id = m_keys[probe_index];

            if (probed_id == TOMBSTONE_SENTINEL || probed_id == EMPTY_SENTINEL) {
                probed_id = new_id;
                std::swap(new_value, m_values[probe_index]);
                m_count++;
                m_longest_probe = dib > m_longest_probe ? dib : m_longest_probe;
                return m_values + probe_index;
            } else if (probed_id == new_id) {
                std::swap(m_values[probe_index], new_value);
                return m_values + probe_index;
            } else {
                //TODO: is this handling wrap-around properly?
                const uint64_t probed_dib = probe_index - (detail::hash_id(probed_id) & N);
                if (probed_dib < dib) {
                    std::swap(probed_id, new_id);
                    std::swap(m_values[probe_index], new_value);
                    m_longest_probe = dib > m_longest_probe ? dib : m_longest_probe;
                    dib = probed_dib;
                }
            }

            probe_index = (probe_index + 1) & N;
            dib++;
        }
    }

    bool remove(EntityID id) {
        const uint64_t N = m_capacity - 1;
        uint64_t probe_index = detail::hash_id(id) & N;

        uint64_t dib = 0;

        while (true) {
            EntityID& probed_id = m_keys[probe_index];

            if (probed_id == id) {
                probed_id = TOMBSTONE_SENTINEL;
                m_values[probe_index].~V();
                m_count--;
                return true;
            } else if (probed_id == EMPTY_SENTINEL) {
                return false;
            }

            probe_index = (probe_index + 1) & N;
            dib++;

            if (dib > m_longest_probe) {
                return false;
            }
        }
    }

    void rehash(size_t new_capacity) {
        assert(new_capacity > m_capacity);

        assert(detail::is_power_of_two(new_capacity) &&
               "EntityMap: Table capacity must be a power of two!");

        EntityMap new_table(new_capacity);

        for (size_t i = 0; i < m_capacity; i++) {
            EntityID id = m_keys[i];
            if (id != EMPTY_SENTINEL && id != TOMBSTONE_SENTINEL) {
                new_table.insert(id, std::move(m_values[i]));
            }
        }

        std::swap(m_keys, new_table.m_keys);
        std::swap(m_values, new_table.m_values);
        std::swap(m_capacity, new_table.m_capacity);
        std::swap(m_longest_probe, new_table.m_longest_probe);
    }

    V& operator[](EntityID id)
    {
        V* result = lookup(id);
        ARK_ASSERT(result != nullptr, "EntityMap: Called operator[] with non-existent EntityID!");
        return *result;
    }

    const V& operator[](EntityID id) const
    {
        const V* result = lookup(id);
        ARK_ASSERT(result != nullptr,
                   "EntityMap: Called operator[] with non-existent EntityID: " << id);
        return *result;
    }
};

} // end namespace ark

