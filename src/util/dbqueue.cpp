/*
@author Robert Griffith
*/

#include "dbqueue.h"

template <typename T>
util::DualBlockingQueue<T>::DualBlockingQueue(size_t capacity)
    : m_capacity(capacity),
      m_clear(false)
{
}

template <typename T>
util::DualBlockingQueue<T>::~DualBlockingQueue()
{
    free();
}

template <typename T>
void util::DualBlockingQueue<T>::initialize()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_initialized = true;
}

template <typename T>
void util::DualBlockingQueue<T>::free()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_initialized = false;
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
    lock.unlock();
    m_cond.notify_all();
}

template <typename T>
void util::DualBlockingQueue<T>::free(void (*op)(T &&value))
{
    if (op == nullptr)
    {
        free();
        return;
    }
    std::unique_lock<std::mutex> lock(m_mtx);
    m_initialized = false;
    while (!m_queue.empty())
    {
        op(std::move(m_queue.front()));
        m_queue.pop();
    }
    lock.unlock();
    m_cond.notify_all();
}

template <typename T>
bool util::DualBlockingQueue<T>::push(const T &value)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cond.wait(lock, [=]
                { return !m_initialized || m_queue.size() < m_capacity });
    if (m_initialized)
    {
        m_queue.push(value);
    }
    lock.unlock();
    m_cond.notify_all();
}

template <typename T>
bool util::DualBlockingQueue<T>::push(T &&value)
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cond.wait(lock, [=]
                { !m_initialized || return m_queue.size() < m_capacity; });
    if (m_initialized)
    {
        m_queue.push(std::move(value));
    }
    lock.unlock();
    m_cond.notify_all();
}

template <typename T>
bool util::DualBlockingQueue<T>::pop(T &value)
{
    bool rv = false;
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cond.wait(lock, [=]
                { return !m_initialized || !m_queue.empty(); });
    if (m_initialized)
    {
        value = std::move(m_queue.front());
        m_queue.pop();
        rv = true;
    }
    lock.unlock();
    m_cond.notify_all();
    return rv;
}
