/*
abstract base class for a parallel thread working on a libav audio context

@author Robert Griffith
*/
#pragma once

#include "context.h"

#include <thread>

namespace whfa
{

    class Threader
    {
    public:
        struct State
        {
            bool run;
            bool terminate;
            int averror;
            int64_t curr_pts; /* presentation timestamp */
        };

        virtual ~Threader();

        virtual void start();
        virtual void stop();
        virtual void pause();

        virtual void get_state(State &state);

    protected:
        Threader(Context &context);

        virtual void thread_func();
        virtual void thread_loop_body() = 0;

        /*
        non-thread-safe operations to set state
        */
        virtual void set_state_start();
        virtual void set_state_stop();
        virtual void set_state_pause();
        virtual void set_state_averror(int averror);

        State m_state;

        Context *m_ctxt;

        std::mutex m_mtx;
        std::condition_variable m_cond;
        std::thread m_thread;
    };

}