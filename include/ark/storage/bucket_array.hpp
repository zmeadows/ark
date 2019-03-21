#pragma once

#include "ark/prelude.hpp"
#include "ark/flat_hash_map.hpp"

#include <array>
#include <chrono>
#include <assert.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace {
constexpr ark::EntityID NO_ENTITY = std::numeric_limits<ark::EntityID>::max();
const uint16_t NO_OPEN_SLOT = 65535;
}

namespace ark {
template <typename T, size_t N>
class BucketArrayStorage;
}

namespace ark::storage {

namespace detail {

void insertion_sort(std::vector<EntityID>& arr)
{
    const int64_t N = arr.size();
    for (int64_t i = 1; i < N; i++) {
        const EntityID key = arr[i];
        int64_t j = i - 1;

        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;
    }
}

} // end namespace detail



template <typename T, size_t N>
class Bucket {
    static_assert(N < 65535 && "Unreasonably large size requested for Bucket.");

    friend class ark::BucketArrayStorage<T,N>;

    T* m_data;
    EntityID* m_slot_ids;
    size_t m_num_active_slots;
    uint16_t m_next_open_slot;

public:
    Bucket(void) : m_data((T*) malloc(sizeof(T) * N))
                 , m_slot_ids((EntityID*) malloc(sizeof(EntityID) * N))
                 , m_num_active_slots(0)
                 , m_next_open_slot(0)
    {
        for (size_t i = 0; i < N; i++) {
            m_slot_ids[i] = NO_ENTITY;
        }
    }

    Bucket(const Bucket&) = delete;
    Bucket(Bucket&&) = delete;
    Bucket& operator=(Bucket&&) = delete;
    Bucket& operator=(const Bucket&) = delete;

    EntityID entity_at_slot(uint16_t slot) const { return m_slot_ids[slot]; }

    ~Bucket(void) {
        for (size_t i = 0; i < N; i++) {
            if (m_slot_ids[i] != NO_ENTITY) {
                m_data[i].~T();
            }
        }

        free(m_data);
        free(m_slot_ids);
        m_num_active_slots = 0;
        m_next_open_slot = NO_OPEN_SLOT;
    }


    inline T& operator[](uint16_t slot) {
        assert(m_slot_ids[slot] != NO_ENTITY && "Attempted to access inactive slot with Bucket::operator[]");
        assert(slot < N && "Attempted to access non-existent slot with Bucket::operator[]");
        return m_data[slot];
    }

    inline const T& operator[](uint16_t slot) const {
        assert(m_slot_ids[slot] != NO_ENTITY && "Attempted to access inactive slot with Bucket::operator[]");
        assert(slot < N && "Attempted to access non-existent slot with Bucket::operator[]");
        return m_data[slot];
    }

    inline bool is_full(void) const { return m_num_active_slots == N; }

    inline size_t num_active_slots(void) const { return m_num_active_slots; }

    template <typename... Args>
    uint16_t insert(EntityID id, Args&&... args) {
        assert(!is_full() && "Attempted to insert item into a full bucket.");
        const uint16_t new_slot = m_next_open_slot;
        new ( &(m_data[new_slot]) ) T(std::forward<Args>(args)...);
        m_slot_ids[new_slot] = id;
        m_num_active_slots++;

        for (uint16_t islot = m_next_open_slot+1; islot < N; islot++) {
            if (m_slot_ids[islot] == NO_ENTITY) {
                m_next_open_slot = islot;
                return new_slot;
            }
        }

        m_next_open_slot = NO_OPEN_SLOT;
        return new_slot;
    }

    void release_slot(size_t slot_index) {
        assert(m_num_active_slots > 0 && "Attempted to release slot from empty Bucket.");
        assert(m_slot_ids[slot_index] != NO_ENTITY && "Attempted to release un-occupied slot from Bucket.");

        m_data[slot_index].~T();
        m_slot_ids[slot_index] = NO_ENTITY;
        m_num_active_slots--;

        if (m_next_open_slot != NO_OPEN_SLOT && slot_index < m_next_open_slot) {
            m_next_open_slot = slot_index;
        } else if (m_next_open_slot == NO_OPEN_SLOT) {
            m_next_open_slot = slot_index;
        }
    }

};

template <typename T, size_t N>
class BucketArray {
    static_assert(N < 65535 && "Unreasonably large Bucket size requested for BucketArray.");

    std::vector<std::unique_ptr<Bucket<T,N>>> m_buckets;

    inline void create_new_bucket(void) {
        m_buckets.emplace_back(new Bucket<T,N>());
    }


public:
    struct Key {
        uint16_t bucket;
        uint16_t slot;
    };

    BucketArray(void) {
        m_buckets.reserve(16);
        create_new_bucket();
    }

    Bucket<T,N>* get_ith_bucket(size_t i) { return m_buckets[i].get(); }
    size_t num_buckets(void) const { return m_buckets.size(); }

    size_t num_filled_slots(void) const {
        size_t sum = 0;
        for (const auto& bucket : m_buckets) {
            sum += bucket->num_active_slots();
        }
        assert(sum < m_buckets.size() * N);
        return sum;
    }

    template <typename... Args>
    const Key insert(Args&&... args) {
        for (uint16_t bucket_id = 0; bucket_id < m_buckets.size(); bucket_id++) {
            auto& bucket = m_buckets[bucket_id];
            if (!bucket->is_full()) {
                return Key {
                    .bucket = bucket_id,
                    .slot = bucket->insert(std::forward<Args>(args)...)
                };
            }
        }

        // If we reach here, we couldn't find an empty slot in the old buckets, so make a new one.
        create_new_bucket();

        return Key {
            .bucket = m_buckets.size() - 1,
            .slot = m_buckets.back()->insert(std::forward<Args>(args)...)
        };
    }

    inline void remove(const Key key) {
        m_buckets[key.bucket]->release_slot(key.slot);
    }

    inline T& operator[](const Key key) {
        assert(key.bucket < m_buckets.size() && "BucketArray::Key points to non-existent Bucket.");
        return m_buckets[key.bucket]->operator[](key.slot);;
    }

    inline const T& operator[](const Key key) const {
        assert(key.bucket < m_buckets.size() && "BucketArray::Key points to non-existent Bucket.");
        return m_buckets[key.bucket]->operator[](key.slot);;
    }
};

} // end namespace ark::storage

namespace ark {

template <typename T, size_t N>
class BucketArrayStorage {
    using Key = typename storage::BucketArray<T,N>::Key;

    storage::BucketArray<T,N> m_array;
    EntityMap<Key> m_keys;
    std::vector<EntityID> m_sort_buffer;

public:
    using ComponentType = T;

    BucketArrayStorage(void) : m_sort_buffer(N) {}

    void defragment(void) {
        const size_t num_buckets = m_array.num_buckets();
        const size_t total_slots = num_buckets * N;
        m_sort_buffer.reserve(total_slots);
        m_sort_buffer.clear();

        // update sort buffer
        for (size_t ibucket = 0; ibucket < num_buckets; ibucket++) {
            auto* bucket = m_array.get_ith_bucket(ibucket);
            for (size_t islot = 0; islot < N; islot++) {
                m_sort_buffer.push_back(bucket->entity_at_slot(islot));
            }
        }

        // use insertion sort here, since the buffer should be nearly sorted already
        storage::detail::insertion_sort(m_sort_buffer);

        // re-order the bucket arrays in order of entity-ids and update EntityID-to-Key map
        for (size_t ibucket = 0; ibucket < m_array.num_buckets(); ibucket++) {
            auto* bucket = m_array.get_ith_bucket(ibucket);
            bucket->m_num_active_slots = 0;
            bucket->m_next_open_slot = NO_OPEN_SLOT;
            for (size_t islot = 0; islot < N; islot++) {
                const EntityID old_entity_at_slot = bucket->entity_at_slot(islot);
                const EntityID new_entity_at_slot = m_sort_buffer[ibucket*N+islot];

                if (new_entity_at_slot != NO_ENTITY) {
                    bucket->m_num_active_slots++;
                } else if (bucket->m_next_open_slot == NO_OPEN_SLOT) {
                    bucket->m_next_open_slot = islot;
                }

                if (old_entity_at_slot != new_entity_at_slot) {
                    Key& new_key = m_keys[new_entity_at_slot];
                    const Key old_key = Key {ibucket, islot};
                    std::swap(m_array[old_key], m_array[new_key]);
                    std::swap(new_key, m_keys[old_entity_at_slot]);
                }
            }
        }
    }

    inline T& get(EntityID id) {
        return m_array[m_keys[id]];
    }

    inline const T& get(EntityID id) const {
        return m_array[m_keys[id]];
    }

    T* get_if(EntityID id) {
        Key* key = m_keys.lookup(id);
        if (key) {
            return std::addressof(m_array[*key]);
        } else {
            return nullptr;
        }
    }

    inline bool has(EntityID id) const {
        return m_keys.lookup(id) != nullptr;
    }

    template <typename... Args>
    T& attach(EntityID id, Args&&... args) {
        assert(!has(id) && "Attempted to attach component to entity that already posesses that component.");
        const Key new_key = m_array.insert(id, std::forward<Args>(args)...);
        m_keys.insert(id, new_key);
        return m_array[new_key];
    }

    inline void detach(EntityID id) {
        const Key old_key = m_keys[id];
        m_array.remove(old_key);
        m_keys.remove(id);
    }
};

}

