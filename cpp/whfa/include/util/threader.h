/**
 * @file util/threader.h
 * @author Robert Griffith
 */
#pragma once

#include "util/error.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace whfa::util
{

    /**
     * @class whfa::util::Threader
     * @brief abstract base class for a threadsafe parallel thread worker
     */
    class Threader
    {
    public:
        /**
         * @struct whfa::util::Threader::State
         * @brief primitives describing thread operation, processing errors, and timestamp
         */
        struct State
        {
            /// @brief true if thread should execute thread_loop_body()
            bool run;
            /// @brief error value
            int error;
            /// @brief timestamp
            int64_t timestamp;
        };

        /**
         * @class whfa::util::Threader::StateHandler
         * @brief class for handling state callbacks
         */
        class StateHandler
        {
        public:
            /**
             * @brief destructor
             */
            virtual ~StateHandler();

            /**
             * @brief handle threader state
             *
             * @param state threader state to handle
             */
            virtual void handle(const State &state) = 0;
        };

        /**
         * @brief destructor that sets state to terminate and joins thread
         */
        virtual ~Threader();

        /**
         * @brief sets state to run thread loop
         *
         * @param handler state handler to be invoked upon internal state changes
         */
        void start(StateHandler *handler = nullptr);

        /**
         * @brief sets state to stop running thread loop and reset timestamp to 0
         */
        void stop();

        /**
         * @brief sets state to stop running thread loop and maintains timestamp
         */
        void pause();

        /**
         * @brief returns copy of state
         *
         * @param[out] state copy of internal State
         */
        void get_state(State &state);

    protected:
        /**
         * @brief protected hidden constructor
         */
        Threader();

        /**
         * @brief pure virtual method to define work to execute in thread
         * @see Threaer::execute_loop()
         */
        virtual void execute_loop_body() = 0;

        /**
         * @brief waits or executes thread loop according to state
         */
        void execute_loop();

        /**
         * @brief returns const access to internal state
         *
         * @return internal state for reading
         */
        const State &get_state() const;

        /**
         * @brief not threadsafe implementation of start()
         * @see Threader::start()
         */
        void set_state_start();

        /**
         * @brief not threadsafe implementation of stop() while setting error
         * @see Threader::stop()
         *
         * @param error error value to set
         */
        void set_state_stop(int error = ENONE);

        /**
         * @brief not threadsafe implementation of pause() while setting error
         * @see Threader::pause()
         *
         * @param error error value to set
         */
        void set_state_pause(int error = ENONE);

        /**
         * @brief not threadsafe method to set state error
         *
         * @param error error value to set
         */
        void set_state_error(int error);

        /**
         * @brief not threadsafe
         */
        void set_state_timestamp(int64_t timestamp);

        /// @brief mutex for synchronizing access to state
        std::mutex _mtx;

    private:
        /// @brief internal state
        State _state;
        /// @brief state callback handler (nullptr if no callback)
        StateHandler *_handler;
        /// @brief true if thread should terminate, hidden logic not exposed in State
        bool _terminate;
        /// @brief codition variable for thread waiting and notifying
        std::condition_variable _cond;
        /// @brief the thread that is running
        std::thread _thread;
    };

}