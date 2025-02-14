// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_THREAD_SAFE_SET_H
#define TURTLECOIN_THREAD_SAFE_SET_H

#include <functional>
#include <set>
#include <shared_mutex>
#include <thread>

template<typename T> class ThreadSafeSet
{
  public:
    ThreadSafeSet() {}

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
    bool contains(const T &key) const
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
    void each(const std::function<void(const T &)> &func) const
    {
        std::shared_lock lock(m_mutex);

        for (const auto &elem : m_container)
        {
            func(elem);
        }
    }

    /**
     * loops over the the container and executes the provided function
     * using each element (without const)
     *
     * @param func
     */
    void eachref(const std::function<void(T &)> &func)
    {
        std::unique_lock lock(m_mutex);

        for (auto &elem : m_container)
        {
            func(elem);
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
    void erase(const T &key)
    {
        std::unique_lock lock(m_mutex);

        m_container.erase(key);
    }

    /**
     * inserts elements
     *
     * @param key
     */
    void insert(const T &key)
    {
        std::unique_lock lock(m_mutex);

        m_container.insert(key);
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
    std::set<T> m_container;

    mutable std::shared_mutex m_mutex;
};

#endif
