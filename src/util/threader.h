/**
 * @file threader.h
 * @author Robert Griffith
 */
#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace util
{

    /**
     * @class util::Threader
     * @brief abstract base class for a threadsafe parallel thread worker
     */
    class Threader
    {
    public:
        /**
         * @struct util::Threader::State
         * @brief primitives describing thread operation, processing errors, and timestamp
         */
        struct State
        {
            /// @brief true if thread should execute thread_loop_body()
            bool run;
            /// @brief true if thread should terminate
            bool terminate;
            /// @brief error value
            int error;
            /// @brief timestamp
            int64_t timestamp;
        };

        /**
         * @brief destructor that sets state to terminate and joins thread
         */
        virtual ~Threader();

        /**
         * @brief sets state to run thread loop
         */
        virtual void start();

        /**
         * @brief sets state to stop running thread loop and reset timestamp to 0
         */
        virtual void stop();

        /**
         * @brief sets state to stop running thread loop and maintains timestamp
         */
        virtual void pause();

        /**
         * @brief returns copy of state
         *
         * @param[out] state copy of internal State
         */
        virtual void get_state(State &state);

    protected:
        /**
         * @brief protected hidden constructor
         */
        Threader();

        /**
         * @brief waits or executes thread loop according to state
         */
        virtual void execute_loop();

        /**
         * @brief pure virtual method to define work to execute in thread
         * @see Threaer::execute_loop()
         */
        virtual void execute_loop_body() = 0;

        /**
         * @brief not threadsafe implementation of start()
         * @see Threader::start()
         */
        virtual void set_state_start();

        /**
         * @brief not threadsafe implementation of stop()
         * @see Threader::stop()
         */
        virtual void set_state_stop();

        /**
         * @brief not threadsafe implementation of pause()
         * @see Threader::pause()
         */
        virtual void set_state_pause();

        /**
         * @brief not threadsafe convenience method to set state error
         */
        virtual void set_state_error(int error);

        /// @brief internal state
        State _state;

        /// @brief mutex for synchronizing access to state
        std::mutex _mtx;
        /// @brief codition variable for thread waiting and notifying
        std::condition_variable _cond;
        /// @brief the thread that is running
        std::thread _thread;
    };

}