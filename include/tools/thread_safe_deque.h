// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_THREAD_SAFE_DEQUE_H
#define TURTLECOIN_THREAD_SAFE_DEQUE_H

#include <deque>
#include <shared_mutex>
#include <thread>
#include <vector>

template<typename T> class ThreadSafeDeque
{
  public:
    ThreadSafeDeque() {}

    /**
     * Returns the element at the specified position in the queue
     *
     * @param position
     * @return
     */
    T operator[](size_t position) const
    {
        std::shared_lock lock(m_mutex);

        return m_container[position];
    }

    /**
     * Returns the element at the specified position in the queue
     *
     * @param position
     * @return
     */
    T at(size_t position) const
    {
        std::shared_lock lock(m_mutex);

        return m_container.at(position);
    }

    /**
     * Returns the last element in the queue
     *
     * @return
     */
    T back() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.back();
    }

    /**
     * Removes all elements from the queue
     */
    void clear()
    {
        std::unique_lock lock(m_mutex);

        m_container.clear();
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
     * Returns whether the queue is empty
     *
     * @return
     */
    bool empty() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.empty();
    }

    /**
     * Returns the first element in the queue without removing it
     *
     * @return
     */
    T front() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.front();
    }

    /**
     * Returns the maximum possible number of elements for the queue
     *
     * @return
     */
    size_t max_size() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.max_size();
    }

    /**
     * Removes the last element in queue and returns it to the caller
     *
     * @return
     */
    T pop_back()
    {
        std::unique_lock lock(m_mutex);

        auto item = m_container.back();

        m_container.pop_back();

        return item;
    }

    /**
     * Removes the first element in queue and returns it to the caller
     *
     * @return
     */
    T pop_front()
    {
        std::unique_lock lock(m_mutex);

        auto item = m_container.front();

        m_container.pop_front();

        return item;
    }

    /**
     * Adds the element to the end of the queue
     *
     * @param item
     */
    void push_back(const T &item)
    {
        std::unique_lock lock(m_mutex);

        m_container.push_back(item);
    }

    /**
     * Adds the vector of elements to the end of the queue
     * in the order in which they were received
     *
     * @param items
     */
    void push_back(const std::vector<T> &items)
    {
        std::unique_lock lock(m_mutex);

        for (const auto &item : items)
        {
            m_container.push_back(item);
        }
    }

    /**
     * Adds the element to the front of the queue
     *
     * @param item
     */
    void push_front(const T &item)
    {
        std::unique_lock lock(m_mutex);

        m_container.push_front(item);
    }

    /**
     * Adds the vector of elements to the front of the queue
     * in the order in which they were received
     *
     * @param items
     * @param preserve_order whether the order should be preserved
     */
    void push_front(const std::vector<T> &items, bool preserve_order = true)
    {
        std::unique_lock lock(m_mutex);

        auto temp = items;

        if (preserve_order)
        {
            std::reverse(temp.begin(), temp.end());
        }

        for (const auto &item : temp)
        {
            m_container.push_front(item);
        }
    }

    /**
     * Changes the number of elements stored in the queue
     *
     * Note: If the new size is less than the current size
     * the additional elements are truncated
     *
     * If the new size is greater than the current size the
     * additional elements are defaulted
     *
     * @param count
     */
    void resize(size_t count)
    {
        std::unique_lock lock(m_mutex);

        m_container.resize(count);
    }

    /**
     * Changes the number of elements stored in the queue
     *
     * Note: If the new size is less than the current size
     * the additional elements are truncated
     *
     * If the new size is greater than the current size the
     * additional elements are set to the specified value
     *
     * @param count
     * @param item
     */
    void resize(size_t count, const T &item)
    {
        std::unique_lock lock(m_mutex);

        m_container.resise(count, item);
    }

    /**
     * Sets the element at the specified position to the
     * value specified
     *
     * @param position
     * @param item
     */
    void set(size_t position, const T &item)
    {
        std::unique_lock lock(m_mutex);

        m_container[position] = item;
    }

    /**
     * Reduces the size of the queue so that it can free unused memory
     */
    void shrink_to_fit()
    {
        std::unique_lock lock(m_mutex);

        m_container.shrink_to_fit();
    }

    /**
     * Returns the size of the queue
     *
     * @return
     */
    size_t size() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.size();
    }

    /**
     * Removes the last element of the queue
     */
    void skip_back()
    {
        std::unique_lock lock(m_mutex);

        if (m_container.empty())
        {
            return;
        }

        m_container.pop_back();
    }

    /**
     * Removes the first element of the queue
     */
    void skip_front()
    {
        std::unique_lock lock(m_mutex);

        if (m_container.empty())
        {
            return;
        }

        m_container.pop_front();
    }


  private:
    std::deque<T> m_container;

    mutable std::shared_mutex m_mutex;
};


#endif
