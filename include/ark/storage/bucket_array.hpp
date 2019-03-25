#pragma once

#include "ark/prelude.hpp"
#include "ark/flat_hash_map.hpp"
#include "ark/third_party/skarupke/ska_sort.hpp"

#include <array>
#include <assert.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <math.h>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
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

template <typename T>
struct is_bucket_array : std::false_type {};

template <typename T, size_t N>
struct is_bucket_array<BucketArrayStorage<T,N>> : std::true_type {};

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
    void set_entity_at_slot(EntityID id, uint16_t slot) {
        m_slot_ids[slot] = id;
    }

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

    // same as operator[], except with no assert for empty slot
    // primarily to be used by the maintanence method in BucketArrayStorage
    inline T& data_at(uint16_t slot) {
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

        inline friend bool operator==(const Key& k1, const Key& k2) {
            return k1.bucket == k2.bucket && k1.slot == k2.slot;
        }

        inline friend bool operator==(const Key& k1, Key& k2) {
            return k1.bucket == k2.bucket && k1.slot == k2.slot;
        }

        inline friend bool operator==(Key& k1, const Key& k2) {
            return k2 == k1;
        }

        inline friend bool operator!=(const Key& k1, Key& k2) {
            return !(k1 == k2);
        }

        inline friend bool operator!=(Key& k1, const Key& k2) {
            return !(k2 == k1);
        }

        friend std::ostream& operator<<(std::ostream& os, const Key& key) {
            os << "[" << key.bucket << "]{" << key.slot << "}";
            return os;
        }
    };

    BucketArray(void) {
        m_buckets.reserve(16);
        create_new_bucket();
    }

    Bucket<T,N>* get_ith_bucket(size_t i) { return m_buckets[i].get(); }
    size_t num_buckets(void) const { return m_buckets.size(); }

    void set_entity_at_key(EntityID id, const Key& key) {
        m_buckets[key.bucket]->set_entity_at_slot(id, key.slot);
    }

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
        return m_buckets[key.bucket]->operator[](key.slot);
    }

    inline const T& operator[](const Key key) const {
        assert(key.bucket < m_buckets.size() && "BucketArray::Key points to non-existent Bucket.");
        return m_buckets[key.bucket]->operator[](key.slot);
    }

    inline T& data_at(const Key key) {
        assert(key.bucket < m_buckets.size() && "BucketArray::Key points to non-existent Bucket.");
        return m_buckets[key.bucket]->data_at(key.slot);
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

    size_t m_removals_since_defrag;

public:
    using ComponentType = T;

    BucketArrayStorage(void) : m_removals_since_defrag(0)
    {}

    double fragmentation_factor(void) const {
        return (double) m_removals_since_defrag / ((double) N * (double) m_array.num_buckets());
    }

    std::optional<double> estimate_maintenance_time(void) const {
        if (fragmentation_factor() > 0.1) {
            return std::log((double)N) * (0.00035 + 3.4e-9 * (double) m_removals_since_defrag);
        } else {
            return {};
        }
    };

    void maintenance(void) {
        m_removals_since_defrag = 0;
        const size_t num_buckets = m_array.num_buckets();
        const size_t total_slots = num_buckets * N;
        m_sort_buffer.reserve(total_slots);
        m_sort_buffer.clear();

        // update sort buffer
        for (size_t ibucket = 0; ibucket < num_buckets; ibucket++) {
            auto* bucket = m_array.get_ith_bucket(ibucket);
            bucket->m_num_active_slots = 0;
            bucket->m_next_open_slot = 0;
            for (size_t islot = 0; islot < N; islot++) {
                m_sort_buffer.push_back(bucket->entity_at_slot(islot));
            }
        }

        ska_sort(m_sort_buffer.begin(), m_sort_buffer.end());

        // re-order the bucket arrays in order of entity-ids and update entity/key meta-data
        for (size_t ibucket = 0; ibucket < m_array.num_buckets(); ibucket++) {
            auto* bucket = m_array.get_ith_bucket(ibucket);
            bucket->m_next_open_slot = NO_OPEN_SLOT;
            for (size_t islot = 0; islot < N; islot++) {
                const EntityID old_entity_at_slot = bucket->entity_at_slot(islot);
                const EntityID new_entity_at_slot = m_sort_buffer[ibucket*N+islot];

                if (new_entity_at_slot != NO_ENTITY) {
                    bucket->m_num_active_slots++;
                } else if (bucket->m_next_open_slot == NO_OPEN_SLOT) {
                    bucket->m_next_open_slot = islot;
                }

                // @OPTIMIZE: I left some unecessarily slow code in here (double map lookups)
                // to make sure it is correct.
                if (old_entity_at_slot != new_entity_at_slot) {
                    const Key focus_key = Key {ibucket, islot};
                    const Key new_entities_current_key = m_keys[new_entity_at_slot];
                    const bool already_swapped = focus_key == new_entities_current_key;
                    // if (already_swapped) {
                    //     std::cout << "already swapped entities: " << new_entity_at_slot << " and " << old_entity_at_slot << std::endl;
                    // }
                    if (!already_swapped) {
                        // const Key new_entities_old_key = m_keys[new_entity_at_slot];
                        // std::cout << "swapping entities: " << new_entity_at_slot << " and " << old_entity_at_slot << std::endl;
                        // std::cout << "at keys: " << new_entities_old_key << " and " << focus_key << std::endl;
                        if (old_entity_at_slot != NO_ENTITY) {
                            const Key old_entities_current_key = m_keys[old_entity_at_slot];

                            ARK_ASSERT(old_entities_current_key == focus_key,
                                       "inconsistency in bucket_array maintenance keys"
                                       << old_entities_current_key << " " << focus_key);

                            m_keys[new_entity_at_slot] = focus_key;
                            m_keys[old_entity_at_slot] = new_entities_current_key;
                            std::swap(m_array.data_at(new_entities_current_key),
                                      m_array.data_at(old_entities_current_key));
                            m_array.set_entity_at_key(new_entity_at_slot, focus_key);
                            m_array.set_entity_at_key(old_entity_at_slot, new_entities_current_key);
                        } else {
                            m_keys[new_entity_at_slot] = focus_key;
                            m_array.data_at(focus_key) = std::move(m_array.data_at(new_entities_current_key));
                            m_array.set_entity_at_key(new_entity_at_slot, focus_key);
                            m_array.set_entity_at_key(NO_ENTITY, new_entities_current_key);
                        }
                    }
                }
            }
        }
    }

    inline T& get(EntityID id) {
        const Key* key = m_keys.lookup(id);
        ARK_ASSERT(key, "Key lookup failed with " << detail::type_name<T>() << " for entity: " << id);
        return m_array[*key];
    }

    inline const T& get(EntityID id) const {
        const Key* key = m_keys.lookup(id);
        ARK_ASSERT(key, "Key lookup failed with " << detail::type_name<T>() << " for entity: " << id);
        return m_array[*key];
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
        ARK_LOG_EVERYTHING("attaching " << detail::type_name<T>() << " to entity: " << id);
        const Key new_key = m_array.insert(id, std::forward<Args>(args)...);
        auto x = m_keys.insert(id, new_key);
        ARK_ASSERT(m_keys.lookup(id), "Failed to insert key into map after attaching " << detail::type_name<T>() << " to entity: " << id << " " << x);
        return m_array[new_key];
    }

    inline void detach(EntityID id) {
        const Key old_key = m_keys[id];
        m_array.remove(old_key);
        m_keys.remove(id);
        m_removals_since_defrag++;
    }
};

}

