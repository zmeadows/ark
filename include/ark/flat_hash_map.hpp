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

constexpr uint32_t hash_id(EntityID id) { return 2*id; }

} // end namespace ark::detail


// A simple open addressing hash table using robin hood hashing
// for mapping EntityIDs to small types like storage handles.
template <typename V>
class EntityMap {

    struct Entry {
        EntityID id;
        V value;
    };

    Entry* m_slots;
    size_t m_count;
    size_t m_capacity;
    uint8_t m_longest_probe;
    double m_max_load_factor;
    static const EntityID EMPTY_SENTINEL = std::numeric_limits<EntityID>::max()-34;
    static const EntityID TOMBSTONE_SENTINEL = std::numeric_limits<EntityID>::max()-35;

public:

    EntityMap(size_t initial_capacity)
        : m_slots( (Entry*) malloc(sizeof(Entry) * initial_capacity) )
        , m_count(0)
        , m_capacity(initial_capacity)
        , m_longest_probe(0)
        , m_max_load_factor(0.5)
    {
        assert(initial_capacity > 0
               && detail::is_power_of_two(initial_capacity)
               && "EntityMap: capacity must always be a power of two!");

        for (size_t i = 0; i < m_capacity; i++) {
            m_slots[i].id = EMPTY_SENTINEL;
        }
    }

    EntityMap() : EntityMap(64) {}

    EntityMap& operator=(const EntityMap&) = delete;
    EntityMap& operator=(EntityMap&&)      = delete;
    EntityMap(EntityMap&&)      = delete;
    EntityMap(const EntityMap&) = delete;

    ~EntityMap(void) {
        for (size_t i = 0; i < m_capacity; i++) {
            Entry& slot = m_slots[i];
            if (slot.id != EMPTY_SENTINEL && slot.id != TOMBSTONE_SENTINEL) {
                slot.value.~V();
            }
        }

        free(m_slots);
        m_slots = nullptr;
        m_count = 0;
        m_capacity = 0;
        m_longest_probe = 0;
    }

    inline size_t size(void) const { return m_count; }

    V* lookup(EntityID lookup_id) const {
        const uint64_t N = m_capacity - 1;
        uint32_t probe_index = detail::hash_id(lookup_id) & N;

        uint64_t dib = 0;

        while (true) {
            Entry& probed_pair = m_slots[probe_index];

            if (probed_pair.id == lookup_id) {
                return std::addressof(probed_pair.value);
            } else if (probed_pair.id == EMPTY_SENTINEL) {
                return nullptr;
            }

            probe_index = (probe_index + 1) & N;
            dib++;

            if (dib > m_longest_probe) return nullptr;
        }
    }

    inline double load_factor() const {
        return static_cast<double>(m_count) / static_cast<double>(m_capacity);
    }

    // TODO: emplace version
    V* insert(EntityID new_id, V new_value) {
        if (load_factor() > m_max_load_factor) {
            rehash(m_capacity * 2);
        }

        const uint64_t N = m_capacity - 1;
        uint32_t probe_index = detail::hash_id(new_id) & N;

        uint64_t dib = 0; // 'd'istance from 'i'nitial 'b'ucket

        while (true) {
            Entry& probed_pair = m_slots[probe_index];

            if (probed_pair.id == TOMBSTONE_SENTINEL || probed_pair.id == EMPTY_SENTINEL) {
                probed_pair.id = new_id;
                probed_pair.value = new_value;
                m_count++;
                m_longest_probe = std::max(dib, (uint64_t) m_longest_probe);
                ARK_ASSERT(m_longest_probe < 100, "Too long probe found!");
                return std::addressof(probed_pair.value);
            } else if (probed_pair.id == new_id) {
                probed_pair.value = new_value;
                return std::addressof(probed_pair.value);
            } else {
                uint64_t probed_dib = probe_index - (detail::hash_id(probed_pair.id) & N);
                if (probed_dib < dib) {
                    m_longest_probe = std::max(dib, (uint64_t) m_longest_probe);
                    ARK_ASSERT(m_longest_probe < 100, "Too long probe found!");
                    std::swap(probed_pair.id, new_id);
                    std::swap(probed_pair.value, new_value);
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
            Entry& probed_slot = m_slots[probe_index];

            if (probed_slot.id == id) {
                probed_slot.id = TOMBSTONE_SENTINEL;
                probed_slot.value.~V();
                m_count--;
                return true;
            } else if (probed_slot.id == EMPTY_SENTINEL) {
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
            Entry& slot = m_slots[i];
            if (slot.id != EMPTY_SENTINEL && slot.id != TOMBSTONE_SENTINEL) {
                new_table.insert(slot.id, slot.value);
            }
        }

        std::swap(m_slots, new_table.m_slots);
        std::swap(m_capacity, new_table.m_capacity);
        std::swap(m_longest_probe, new_table.m_longest_probe);
    }

    V& operator[](EntityID id)
    {
        V* result = lookup(id);
        assert(result != nullptr && "EntityMap: Called operator[] with non-existent EntityID!");
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
