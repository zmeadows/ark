#pragma once

#include "ark/prelude.hpp"

#include <algorithm>
#include <vector>

namespace ark {

namespace detail {

// from https://jeaf.org/blog/inplace-set_difference/
template <class It1, class It2>
It1 inplace_set_difference(It1 first1, It1 last1, It2 first2, It2 last2)
{
    // Find the first element in range1 that is not smaller than the first
    // element to remove
    if (first2 != last2)
    {
        first1 = std::lower_bound(first1, last1, *first2);
    }

    // Move elements that are not in range2
    It1 result = first1;
    while (first1 != last1 && first2 != last2)
    {
        if      (*first1 < *first2) *result++ = *first1++;
        else if (*first2 < *first1) ++first2;
        else
        {
            ++first1;
            ++first2;
        }
    }

    // Copy the remaining elements, and return an iterator that points to the end
    // of the new range
    while (first1 != last1) *result++ = *first1++;
    return result;
}

}

class FlatEntitySet {
    std::vector<EntityID> m_entities;
public:
    inline void reserve(size_t capacity) { m_entities.reserve(capacity); }
    inline size_t size(void) const { return m_entities.size(); }

    void insert_entities(const std::vector<EntityID>& more_entities) {
        m_entities.reserve(m_entities.size() + more_entities.size());

        const size_t merge_point = m_entities.size();

        std::copy(more_entities.begin(), more_entities.end(), std::back_inserter(m_entities));

        std::inplace_merge(m_entities.begin(), m_entities.begin() + merge_point, m_entities.end());

        // OPTIMIZE: in principle this call is not needed, as we will never attempt to insert
        // entities into a set that already contains them.
        m_entities.erase(std::unique(m_entities.begin(), m_entities.end()), m_entities.end());
    }

    void insert_new_entities(const std::vector<EntityID>& new_entities) {
        m_entities.reserve(m_entities.size() + new_entities.size());
        m_entities.insert(m_entities.end(), new_entities.begin(), new_entities.end());
    }

    void remove_entities(const std::vector<EntityID>& entities_to_remove) {
        auto new_end = detail::inplace_set_difference(m_entities.begin(),
                                                      m_entities.end(),
                                                      entities_to_remove.begin(),
                                                      entities_to_remove.end());
        m_entities.erase(new_end, m_entities.end());
    }


    inline bool contains(EntityID id) {
        return std::binary_search(m_entities.begin(), m_entities.end(), id);
    }

    using const_iterator = decltype(m_entities)::const_iterator;
    const_iterator cbegin(void) const { return m_entities.cbegin(); }
    const_iterator cend(void) const { return m_entities.cend(); }

    const_iterator begin(void) const { return m_entities.cbegin(); }
    const_iterator end(void) const { return m_entities.cend(); }

    EntityID* begin_ptr(void) { return m_entities.data(); }
    EntityID* end_ptr(void) { return m_entities.data() + m_entities.size(); }
};


}
