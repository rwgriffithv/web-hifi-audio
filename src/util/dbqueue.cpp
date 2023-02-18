/*
@author Robert Griffith
*/

#include "dbqueue.h"

template <typename T>
util::DualBlockingQueue<T>::DualBlockingQueue(size_t capacity)
    : m_capacity(capacity),
      m_flush(false),
      m_num_wait(0)
{
}

template <typename T>
util::DualBlockingQueue<T>::~DualBlockingQueue()
{
    flush();
}

template <typename T>
void util::DualBlockingQueue<T>::flush()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_flush = true;
    while (!m_queue.empty())
    {
        m_queue.pop();
    }
    lock.unlock();
    m_cond.notify_all();
}

template <typename T>
void util::DualBlockingQueue<T>::flush(void (*op)(T &&value))
{
    if (op == nullptr)
    {
        flush();
        return;
    }
    std::unique_lock<std::mutex> lock(m_mtx);
    m_flush = true;
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
    bool rv = false;
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_flush)
    {
        return rv;
    }
    m_num_wait++;
    m_cond.wait(lock, [=]
                { return m_flush || m_queue.size() < m_capacity; });
    m_num_wait--;
    if (m_flush)
    {
        m_flush = m_num_wait != 0;
    }
    else
    {
        m_queue.push(value);
        rv = true;
    }
    lock.unlock();
    m_cond.notify_all();
    return rv;
}

template <typename T>
bool util::DualBlockingQueue<T>::push(T &&value)
{
    bool rv = false;
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_flush)
    {
        return rv;
    }
    m_num_wait++;
    m_cond.wait(lock, [=]
                { return m_flush || m_queue.size() < m_capacity; });
    m_num_wait--;
    if (m_flush)
    {
        m_flush = m_num_wait != 0;
    }
    else
    {
        m_queue.push(std::move(value));
        rv = true;
    }
    lock.unlock();
    m_cond.notify_all();
    return rv;
}

template <typename T>
bool util::DualBlockingQueue<T>::pop(T &value)
{
    bool rv = false;
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_flush)
    {
        return rv;
    }
    m_num_wait++;
    m_cond.wait(lock, [=]
                { return m_flush || !m_queue.empty(); });
    m_num_wait--;
    if (m_flush)
    {
        m_flush = m_num_wait != 0;
    }
    else
    {
        value = std::move(m_queue.front());
        m_queue.pop();
        rv = true;
    }
    lock.unlock();
    m_cond.notify_all();
    return rv;
}

template <typename T>
const size_t util::DualBlockingQueue<T>::capacity()
{
    return m_capacity;
}

template <typename T>
const size_t util::DualBlockingQueue<T>::size()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_queue.size();
}
