// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_THREAD_SAFE_MAP_H
#define TURTLECOIN_THREAD_SAFE_MAP_H

#include <functional>
#include <map>
#include <shared_mutex>
#include <thread>

template<typename L, typename R> class ThreadSafeMap
{
  public:
    ThreadSafeMap() {}

    /**
     * Returns the element at the specified key in the container
     *
     * @param key
     * @return
     */
    R at(L key) const
    {
        std::shared_lock lock(m_mutex);

        return m_container.at(key);
    }

    /**
     * Removes all elements from the container
     */
    void clear()
    {
        std::unique_lock lock(m_mutex);

        m_container.clear();
    }

    /**
     * checks if the container contains element with specific key
     *
     * @param key
     * @return
     */
    bool contains(const L &key) const
    {
        std::shared_lock lock(m_mutex);

        return m_container.count(key) != 0;
    }

    /**
     * loops over the the container and executes the provided function
     * using each element
     *
     * @param func
     */
    void each(const std::function<void(const L &, const R &)> &func) const
    {
        std::shared_lock lock(m_mutex);

        for (const auto &[key, value] : m_container)
        {
            func(key, value);
        }
    }

    /**
     * loops over the the container and executes the provided function
     * using each element (without const)
     *
     * @param func
     */
    void eachref(const std::function<void(L &, R &)> &func)
    {
        std::unique_lock lock(m_mutex);

        for (auto &[key, value] : m_container)
        {
            func(key, value);
        }
    }

    /**
     * Returns whether the container is empty
     *
     * @return
     */
    bool empty() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.empty();
    }

    /**
     * erases elements
     *
     * @param key
     */
    void erase(const L &key)
    {
        std::unique_lock lock(m_mutex);

        m_container.erase(key);
    }

    /**
     * inserts elements
     *
     * @param key
     * @param value
     */
    void insert(const L &key, const R &value)
    {
        std::unique_lock lock(m_mutex);

        m_container.insert({key, value});
    }

    /**
     * inserts elements
     *
     * @param kv
     */
    void insert(const std::tuple<L, R> &kv)
    {
        std::unique_lock lock(m_mutex);

        m_container.insert(kv);
    }

    /**
     * inserts an element or assigns to the current element if the key already exists
     *
     * @param key
     * @param value
     */
    void insert_or_assign(const L &key, const R &value)
    {
        std::unique_lock lock(m_mutex);

        m_container.insert_or_assign(key, value);
    }

    /**
     * inserts an element or assigns to the current element if the key already exists
     *
     * @param kv
     */
    void insert_or_assign(const std::tuple<L, R> &kv)
    {
        std::unique_lock lock(m_mutex);

        const auto [key, value] = kv;

        m_container.insert_or_assign(key, value);
    }

    /**
     * Returns the maximum possible number of elements for the container
     *
     * @return
     */
    size_t max_size() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.max_size();
    }

    /**
     * Returns the size of the container
     *
     * @return
     */
    size_t size() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.size();
    }

  private:
    std::map<L, R> m_container;

    mutable std::shared_mutex m_mutex;
};

#endif
