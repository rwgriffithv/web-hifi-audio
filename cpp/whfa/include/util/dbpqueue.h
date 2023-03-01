/**
 * @file dbpqueue.h
 * @author Robert Griffith
 */
#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace whfa::util
{

    /**
     * @class whfa::util::DBPQueue<T>
     * @brief threadsafe dual-blocking pointer queue of fixed capacity
     *
     * only stores pointers T*
     * popping blocks when queue is empty
     * pushing blocks when queue is full
     * implemented using two arrays with separate locks for popping and pushing
     * effective capacity at worst is 2 * specified capacity
     */
    template <typename T>
    class DBPQueue
    {
    public:
        /// @brief callback type to be invoked when flushing (stateless, mostly for destruction)
        using FlushCallback = void (*)(T *);

        /**
         * @brief constructor
         *
         * @param capacity the ideal target capacity
         * @param callback default callback to invoke when flushing
         */
        DBPQueue(size_t capacity, FlushCallback callback = nullptr)
            : _capacity(capacity),
              _callback(callback),
              _pop_buf({.buf = new T *[capacity],
                        .pos = 0,
                        .sz = 0}),
              _pop_st({.flush = false,
                       .num_wait = 0}),
              _push_buf({.buf = new T *[capacity],
                         .pos = 0,
                         .sz = 0}),
              _push_st({.flush = false,
                        .num_wait = 0})
        {
        }

        /**
         * @brief destructor
         */
        ~DBPQueue()
        {
            flush();
            {
                std::lock_guard<std::mutex> pop_lk(_pop_mtx);
                delete _pop_buf.buf;
            }
            {
                std::lock_guard<std::mutex> push_lk(_push_mtx);
                delete _push_buf.buf;
            }
        }

        /**
         * @brief clear queue and handle invoked on each popped element
         *
         * notify all waiting threads
         * waiting push and pop calls will return false
         * callback primarily required for proper destruction and freeing of heap memory
         * ideally only need to use default callback specified during construction
         *
         * @param callback function to invoked on popped pointers, nullptr = use default
         */
        void flush(FlushCallback callback = nullptr)
        {
            FlushCallback cb = (callback == nullptr) ? _callback : callback;

            {
                std::lock_guard<std::mutex> lk(_pop_mtx);
                _pop_st.flush = true;
                if (cb != nullptr)
                {
                    while (_pop_buf.sz != 0)
                    {
                        cb(_pop_buf.buf[_pop_buf.pos++]);
                        _pop_buf.sz--;
                    }
                }
                _pop_buf.pos = 0;
            }
            _pop_cond.notify_all();

            {
                std::lock_guard<std::mutex> lk(_push_mtx);
                _push_st.flush = true;
                if (cb != nullptr)
                {
                    size_t i = 0;
                    while (i != _push_buf.sz)
                    {
                        cb(_push_buf.buf[i++]);
                    }
                }
                _push_buf.sz = 0;
            }
            _push_cond.notify_all();
        }

        /**
         * @brief remove and retrieve first element in queue
         *
         * @param[out] ptr pointer moved from front of queue
         * @return true if successful, false if flushing or flushed while waiting
         */
        bool pop(T *&ptr)
        {
            bool rv = false;
            std::unique_lock<std::mutex> pop_lk(_pop_mtx);
            while (!_pop_st.flush && _pop_buf.sz == 0)
            {
                bool refilled;
                {
                    std::lock_guard<std::mutex> push_lk(_push_mtx);
                    refilled = fill_pop_buffer();
                }
                if (refilled)
                {
                    _push_cond.notify_all();
                }
                else
                {
                    _pop_st.num_wait++;
                    _pop_cond.wait(pop_lk);
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
            pop_lk.unlock();
            return rv;
        }

        /**
         * @brief remove and retrieve first element in queue within a timeout period
         *
         * @param[out] ptr pointer moved from front of queue
         * @param timeout max duration to wait for
         * @return true if successful, false if flushed or timeout reached
         */
        template <typename Rep, typename Period>
        bool pop(T *&ptr, const std::chrono::duration<Rep, Period> &timeout)
        {
            bool rv = false;
            std::unique_lock<std::mutex> pop_lk(_pop_mtx);
            while (!_pop_st.flush && _pop_buf.sz == 0)
            {
                bool refilled;
                {
                    std::lock_guard<std::mutex> push_lk(_push_mtx);
                    refilled = fill_pop_buffer();
                }
                if (refilled)
                {
                    _push_cond.notify_all();
                }
                else
                {
                    _pop_st.num_wait++;
                    const std::cv_status cvs = _pop_cond.wait_for(pop_lk, timeout);
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
            pop_lk.unlock();
            return rv;
        }

        /**
         * @brief insert element into back of queue
         *
         * @param ptr pointer to copy to queue
         * @return true if successful, false if flushing or flushed while waiting
         */
        bool push(T *ptr)
        {
            bool rv = false;
            std::unique_lock<std::mutex> push_lk(_push_mtx);
            while (!_push_st.flush && _push_buf.sz == _capacity)
            {
                _push_st.num_wait++;
                _push_cond.wait(push_lk);
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
            push_lk.unlock();
            _pop_cond.notify_all();
            return rv;
        }

        /**
         * @brief insert element into back of queue within a timeout period
         *
         * @param ptr pointer to copy to queue
         * @param timeout max duration to wait for
         * @return true if successful, false if flushed or timeout reached
         */
        template <typename Rep, typename Period>
        bool push(T *ptr, const std::chrono::duration<Rep, Period> &timeout)
        {
            bool rv = false;
            std::unique_lock<std::mutex> push_lk(_push_mtx);
            while (!_push_st.flush && _push_buf.sz == _capacity)
            {
                _push_st.num_wait++;
                const std::cv_status cvs = _push_cond.wait_for(push_lk, timeout);
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
            push_lk.unlock();
            _pop_cond.notify_all();
            return rv;
        }

        /**
         * @brief get ideal capacity of queue specified from constructor
         *
         * backed by two arrays of this size with a worst case max of 2 * capacity
         *
         * @return capacity of queue
         */
        const size_t get_capacity()
        {
            return _capacity;
        }

        /**
         * @brief get number of items in queue
         *
         * @return number of items in queue
         */
        const size_t get_size()
        {
            std::lock_guard<std::mutex> pop_lk(_pop_mtx);
            std::lock_guard<std::mutex> push_lk(_push_mtx);
            return _pop_buf.sz + _push_buf.sz;
        }

    protected:
        /**
         * @struct whfa::util::DBPQueue<T>::Buffer
         * @brief struct for holding backing buffer data
         */
        struct BufferData
        {
            /// @brief array of pointers
            T **buf;
            /// @brief position in buffer where valid data starts
            size_t pos;
            /// @brief number of valid elements in buffer
            size_t sz;
        };

        /**
         * @struct whfa::util::DBPQueue<T>::State
         * @brief struct for holding backing buffer state
         */
        struct BufferState
        {
            /// @brief control flag to signal a flush
            bool flush;
            /// @brief number of threads waiting to access buffer
            size_t num_wait;
        };

        /**
         * @brief not thread safe filling of pop buffer data with available data from push buffer
         *
         * if push buffer has data, swaps array pointers and sizes, sets push buffer size to zero
         *
         * @return true if buffers swapped, false if push buffer was empty
         */
        bool fill_pop_buffer()
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

        /// @brief capacity of each underlying buffer
        size_t _capacity;
        /// @brief default flush callback to invoke
        FlushCallback _callback;

        /// @brief pop buffer data
        BufferData _pop_buf;
        /// @brief pop buffer state
        BufferState _pop_st;
        /// @brief mutex for synchronizing pop buffer access
        std::mutex _pop_mtx;
        /// @brief condition variable for waiting and notifying blocking pop threads
        std::condition_variable _pop_cond;

        /// @brief push buffer data
        BufferData _push_buf;
        /// @brief push buffer state
        BufferState _push_st;
        /// @brief mutex for synchronizing push buffer access
        std::mutex _push_mtx;
        /// @brief condition variable for waiting and notifying blocking push threads
        std::condition_variable _push_cond;
    };

}