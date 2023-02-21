/**
 * @file dbpqueue.cpp
 * @author Robert Griffith
 */
#include "dbpqueue.h"

/**
 * util::DBPQueue<T> public methods
 */

template <typename T>
util::DBPQueue<T>::DBPQueue(size_t capacity)
    : _capacity(capacity),
      _push_buf({.buf = new T *[capacity],
                 .pos = 0,
                 .sz = 0}),
      _push_st({.flush = false,
                .num_wait = 0}),
      _pop_buf({.buf = new T *[capacity],
                .pos = 0,
                .sz = 0}),
      _pop_st({.flush = false,
               .num_wait = 0})
{
}

template <typename T>
util::DBPQueue<T>::~DBPQueue()
{
    flush();
    delete _push_buf.buf;
    delete _pop_buf.buf;
}

template <typename T>
void util::DBPQueue<T>::flush()
{

    {
        std::lock_guard<std::mutex>(_pop_mtx);
        _pop_st.flush = true;
        _pop_buf.pos = 0;
        _pop_buf.sz = 0;
    }
    _pop_cond.notify_all();

    {
        std::lock_guard<std::mutex>(_push_mtx);
        _push_st.flush = true;
        _push_buf.sz = 0;
    }
    _push_cond.notify_all();
}

template <typename T>
void util::DBPQueue<T>::flush(void (*handle)(T *ptr))
{
    if (handle == nullptr)
    {
        flush();
        return;
    }

    {
        std::lock_guard<std::mutex>(_pop_mtx);
        _pop_st.flush = true;
        while (_pop_buf.pos != _pop_buf.sz)
        {
            handle(_pop_buf.buf[_pop_buf.pos++]);
        }
        _pop_buf.pos = 0;
        _pop_buf.sz = 0;
    }
    _pop_cond.notify_all();

    {
        std::lock_guard<std::mutex>(_push_mtx);
        _push_st.flush = true;
        size_t i = 0;
        while (i != _push_buf.sz)
        {
            op(_push_buf[i++]);
        }
        _push_buf.sz = 0;
    }
    _push_cond.notify_all();
}

template <typename T>
bool util::DBPQueue<T>::pop(T *&ptr)
{
    bool rv = false;
    std::unique_lock<std::mutex> pop_lock(_pop_mtx);
    while (!_pop_st.flush && _pop_buf.sz == 0)
    {
        const bool refilled;
        {
            std::lock_guard<std::mutex> push_lock(_push_mtx);
            refilled = fill_pop_buffer();
        }
        if (refilled)
        {
            _push_cond.notify_all();
        }
        else
        {
            _pop_st.num_wait++;
            _pop_cond.wait(pop_lock);
            _pop_st.num_wait--;
        }
    }
    if (_pop_st.flush)
    {
        _pop_st.flush = _pop_st.num_wait != 0;
    }
    else
    {
        ptr = _pop_buf.buf[_pop_buf.pos++];
        _pop_buf.sz--;
        rv = true;
    }
    pop_lock.unlock();
    return rv;
}

template <typename T>
template <typename Rep, typename Period>
bool util::DBPQueue<T>::pop(T *&ptr, const std::chrono::duration<Rep, Period> &timeout)
{
    bool rv = false;
    std::unique_lock<std::mutex> pop_lock(_pop_mtx);
    while (!_pop_st.flush && _pop_buf.sz == 0)
    {
        const bool refilled;
        {
            std::lock_guard<std::mutex> push_lock(_push_mtx);
            refilled = fill_pop_buffer();
        }
        if (refilled)
        {
            _push_cond.notify_all();
        }
        else
        {
            _pop_st.num_wait++;
            const std::cv_status cvs = _pop_cond.wait_for(pop_lock, timeout);
            _pop_st.num_wait--;
            if (cvs == std::cv_status::timeout)
            {
                return false;
            }
        }
    }
    if (_pop_st.flush)
    {
        _pop_st.flush = _pop_st.num_wait != 0;
    }
    else
    {
        ptr = _pop_buf.buf[_pop_buf.pos++];
        _pop_buf.sz--;
        rv = true;
    }
    pop_lock.unlock();
    return rv;
}

template <typename T>
bool util::DBPQueue<T>::push(const T *ptr)
{
    bool rv = false;
    std::unique_lock<std::mutex> push_lock(_push_mtx);
    while (!_push_st.flush && _push_buf.sz == _capacity)
    {
        _push_st.num_wait++;
        _push_cond.wait(push_lock);
        _push_st.num_wait--;
    }
    if (_push_st.flush)
    {
        _push_st.flush = _push_st.num_wait != 0;
    }
    else
    {
        _push_buf.buf[_push_buf.sz++] = ptr;
        rv = true;
    }
    push_lock.unlock();
    _pop_cond.notify_all();
    return rv;
}

template <typename T>
template <typename Rep, typename Period>
bool util::DBPQueue<T>::push(const T *ptr, const std::chrono::duration<Rep, Period> &timeout)
{
    bool rv = false;
    std::unique_lock<std::mutex> push_lock(_push_mtx);
    while (!_push_st.flush && _push_buf.sz == _capacity)
    {
        _push_st.num_wait++;
        const std::cv_status cvs = _push_cond.wait_for(push_lock, timeout);
        _push_st.num_wait--;
        if (cvs == std::cv_status::timeout)
        {
            return false;
        }
    }
    if (_push_st.flush)
    {
        _push_st.flush = _push_st.num_wait != 0;
    }
    else
    {
        _push_buf.buf[_push_buf.sz++] = ptr;
        rv = true;
    }
    push_lock.unlock();
    _pop_cond.notify_all();
    return rv;
}

template <typename T>
const size_t util::DBPQueue<T>::get_capacity()
{
    return _capacity;
}

template <typename T>
const size_t util::DBPQueue<T>::get_size()
{
    std::lock_guard<std::mutex> pop_lock(_pop_mtx);
    std::lock_guard<std::mutex> push_lock(_push_mtx);
    return _pop_buf.sz + _push_buf.sz;
}

/**
 * util::DBPQueue<T> protected methods
 */

template <typename T>
bool util::DBPQueue<T>::fill_pop_buffer()
{
    if (_push_buf.sz == 0)
    {
        return false;
    }
    T **b = _pop_buf.buf;
    _pop_buf = _push_buf;
    _push_buf.buf = b;
    _push_buf.sz = 0;
    return true;
}
