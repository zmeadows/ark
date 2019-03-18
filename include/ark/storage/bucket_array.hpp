#pragma once

#include "ark/types.hpp"
#include "ark/third_party/skarupke/bytell_hash_map.hpp"

#include <assert.h>
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace ark::storage {

//TODO: define iterators for Bucket/BucketArray

template <typename T, size_t N>
class Bucket {
    static_assert(N < 65535 && "Unreasonably large size requested for Bucket.");

    static constexpr uint16_t NO_OPEN_SLOT = 65535;

    std::array<bool, N> m_occupied;
    std::unique_ptr<T[]> m_data;
    size_t m_num_active_slots;
    uint16_t m_next_open_slot;

public:
    Bucket(void) : m_occupied({false})
                 , m_data(new T[N])
                 , m_num_active_slots(0)
                 , m_next_open_slot(0)
                 {}

    inline T& operator[](uint16_t slot) {
        assert(m_occupied[slot] && "Attempted to access inactive slot with Bucket::operator[]");
        assert(slot < N && "Attempted to access non-existent slot with Bucket::operator[]");
        return m_data[slot];
    }

    inline const T& operator[](uint16_t slot) const {
        assert(m_occupied[slot] && "Attempted to access inactive slot with Bucket::operator[]");
        assert(slot < N && "Attempted to access non-existent slot with Bucket::operator[]");
        return m_data[slot];
    }

    inline bool is_full(void) const { return m_num_active_slots == N; }

    inline size_t num_active_slots(void) const { return m_num_active_slots; }

    template <typename... Args>
    uint16_t insert(Args&&... args) {
        assert(!is_full() && "Attempted to insert item into a full bucket.");
        const uint16_t new_slot = m_next_open_slot;
        new ( &(m_occupied[new_slot]) ) T(std::forward<Args>(args)...);
        m_occupied[new_slot] = true;
        m_num_active_slots++;

        if (is_full()) {
            m_next_open_slot = NO_OPEN_SLOT;
        } else {
            assert(m_next_open_slot < N-1);
            for (uint16_t islot = m_next_open_slot+1; islot < N; islot++) {
                if (!m_occupied[islot]) {
                    m_next_open_slot = islot;
                    break;
                }
            }
        }

        return new_slot;
    }

    void release_slot(size_t slot_index) {
        assert(m_num_active_slots > 0 && "Attempted to release slot from empty Bucket.");
        assert(m_occupied[slot_index] && "Attempted to release un-occupied slot from Bucket.");

        m_data[slot_index].~T();
        m_occupied[slot_index] = false;
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

public:
    struct Key {
        uint16_t bucket;
        uint16_t slot;
    };

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
        m_buckets.emplace_back();

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

    BucketArray() {
        m_buckets.reserve(16);
    }

};

} // end namespace ark::storage

namespace ark {

template <typename T, size_t N>
class BucketArrayStorage {
    using Key = typename storage::BucketArray<T,N>::Key;

    storage::BucketArray<T,N> m_array;
    mutable ska::bytell_hash_map<EntityID, Key> m_keys;

public:
    using ComponentType = T;

    inline T& get(EntityID id) {
        return m_array[m_keys[id]];
    }

    inline const T& get(EntityID id) const {
        return m_array[m_keys[id]];
    }

    T* get_if(EntityID id) {
        auto it = m_keys.find(id);
        if (it == m_keys.end()) {
            return nullptr;
        } else {
            return std::addressof(m_array[it->second]);
        }
    }

    inline bool has(EntityID id) const {
        return m_keys.find(id) != m_keys.end();
    }

    template <typename... Args>
    T& attach(EntityID id, Args&&... args) {
        assert(!has(id) && "Attempted to attach component to entity that already posesses that component.");
        const Key new_key = m_array.insert(std::forward<Args>(args)...);
        m_keys.insert_or_assign(id, new_key);
        return m_array[new_key];
    }

    inline void detach(EntityID id) {
        m_array.remove(m_keys[id]);
        m_keys.erase(id);
    }
};

}

