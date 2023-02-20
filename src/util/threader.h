/*
abstract base class for a parallel thread worker

@author Robert Griffith
*/
#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace util
{

    class Threader
    {
    public:
        struct State
        {
            bool run;
            bool terminate;
            int error;
            int64_t curr_pts; /* presentation timestamp */
        };

        virtual ~Threader();

        virtual void start();
        virtual void stop();
        virtual void pause();

        virtual void get_state(State &state);

    protected:
        Threader();

        virtual void thread_func();
        virtual void thread_loop_body() = 0;

        /*
        non-thread-safe operations to set state
        */
        virtual void set_state_start();
        virtual void set_state_stop();
        virtual void set_state_pause();
        virtual void set_state_error(int error);

        State _state;

        std::mutex _mtx;
        std::condition_variable _cond;
        std::thread _thread;
    };

}