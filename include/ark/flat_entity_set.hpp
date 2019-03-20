#pragma once

#include "ark/prelude.hpp"

namespace ark {

namespace detail {

struct BinarySearchResult {
    enum class Type {
        InsertIndex,
        AlreadyExists
    } type;

    int64_t index = -1;
};

BinarySearchResult binary_search(const std::vector<EntityID>& entities, EntityID search_id) {
    BinarySearchResult result;

    // We perform an explicit check here, because in the vast majority of cases
    // we will be inserting a newly created entity, which means that the entity ID
    // will be greater than any other EntityID already in the set
    if (entities.size() == 0 || search_id > entities.back()) {
        result.type = BinarySearchResult::Type::InsertIndex;
        result.index = entities.size();
        return result;
    }

    int64_t low = 0;
    int64_t high = entities.size() - 1;

    do {
        const int64_t mid = low + (high - low) / 2;
        const EntityID mid_value = entities[mid];

        if (search_id == mid_value) {
            result.type = BinarySearchResult::Type::AlreadyExists;
            result.index = (size_t) mid;
            return result;
        } else if (search_id < mid_value) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    } while (high >= low);

    result.type = BinarySearchResult::Type::InsertIndex;
    result.index = (size_t) low;

    return result;
}

}

class FlatEntitySet {
    std::vector<EntityID> m_entities;
public:
    inline void reserve(size_t capacity) { m_entities.reserve(capacity); }
    inline size_t size(void) const { return m_entities.size(); }

    void insert(EntityID new_id) {
        const detail::BinarySearchResult result = detail::binary_search(m_entities, new_id);

        switch (result.type) {
            case detail::BinarySearchResult::Type::InsertIndex:
                m_entities.insert(m_entities.begin() + result.index, new_id);
                break;
            case detail::BinarySearchResult::Type::AlreadyExists:
                break;
        }
    }

    void remove(const FlatEntitySet& ids) {
        std::vector<EntityID> diff;
        diff.reserve(m_entities.size());

        std::set_difference(m_entities.begin(), m_entities.end(), ids.begin(), ids.end(),
                            std::inserter(diff, diff.begin()));

        std::swap(diff, m_entities);
    }

    inline bool contains(EntityID id) {
        return detail::binary_search(m_entities, id).type == detail::BinarySearchResult::Type::AlreadyExists;
    }

    using const_iterator = decltype(m_entities)::const_iterator;
    const_iterator begin(void) const { return m_entities.cbegin(); }
    const_iterator end(void) const { return m_entities.cend(); }
};

// a const view into an EntitySet for iterating over inside Systems
class FollowedEntities {
    const FlatEntitySet* m_set;
public:
    FlatEntitySet::const_iterator begin(void) const { return m_set->begin(); }
    FlatEntitySet::const_iterator end(void) const { return m_set->end(); }

    inline size_t size(void) const { return m_set->size(); }

    FollowedEntities(const FlatEntitySet* s) : m_set(s) {}
};

}
