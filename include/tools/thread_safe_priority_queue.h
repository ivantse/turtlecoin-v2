// Copyright (c) 2021, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef TURTLECOIN_THREAD_SAFE_PRIORITY_QUEUE_H
#define TURTLECOIN_THREAD_SAFE_PRIORITY_QUEUE_H

#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

template<typename T, typename Comparison = std::less<T>> class ThreadSafePriorityQueue
{
  public:
    ThreadSafePriorityQueue() {}

    /**
     * Removes all elements from the container
     */
    void clear()
    {
        std::unique_lock lock(m_mutex);

        while (!m_container.empty())
        {
            m_container.pop();
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
     * Removes the top element in queue and returns it to the caller
     *
     * @return
     */
    T pop()
    {
        std::unique_lock lock(m_mutex);

        auto item = m_container.top();

        m_container.pop();

        return item;
    }

    /**
     * Adds the element to the queue
     *
     * @param item
     */
    void push(const T &item)
    {
        std::unique_lock lock(m_mutex);

        m_container.push(item);
    }

    /**
     * Adds the vector of elements to the queue
     * in the order in which they were received
     *
     * @param items
     */
    void push(const std::vector<T> &items)
    {
        std::unique_lock lock(m_mutex);

        for (const auto &item : items)
        {
            m_container.push(item);
        }
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
     * Returns the top element in the queue without removing it
     *
     * @return
     */
    T top() const
    {
        std::shared_lock lock(m_mutex);

        return m_container.top();
    }

  private:
    std::priority_queue<T, std::vector<T>, Comparison> m_container;

    mutable std::shared_mutex m_mutex;
};

#endif
