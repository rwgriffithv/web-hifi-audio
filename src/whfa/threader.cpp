#include "threader.h"

/*
public methods
*/

whfa::Threader::~Threader()
{
    std::unique_lock<std::mutex> lock(m_mtx);
    m_state.terminate = true;
    lock.unlock();
    m_cond.notify_one();
    m_thread.join();
}

void whfa::Threader::start()
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        set_state_start();
    }
    m_cond.notify_one();
}

void whfa::Threader::stop()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    set_state_stop();
}

void whfa::Threader::pause()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    set_state_pause();
}

void whfa::Threader::get_state(State &state)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    state = m_state;
}

/*
protected methods
*/

whfa::Threader::Threader(whfa::Context &context)
    : m_state({.run = false,
               .terminate = false,
               .averror = 0,
               .curr_pts = 0}),
      m_ctxt(&context),
      m_thread(&Threader::thread_func, this)
{
}

void whfa::Threader::thread_func()
{
    do
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(
            lock, [=]
            { return m_state.run || m_state.terminate; });
        // entire loop body remains locked
        if (m_state.terminate)
        {
            break;
        }

        thread_loop_body();

        lock.unlock();
    } while (true);
}

void whfa::Threader::set_state_start()
{
    m_state.run = true;
    m_state.averror = 0;
    m_state.curr_pts = 0;
}

void whfa::Threader::set_state_stop()
{
    m_state.run = false;
    m_state.curr_pts = 0;
}

void whfa::Threader::set_state_pause()
{
    m_state.run = false;
}

void whfa::Threader::set_state_averror(int averror)
{
    m_state.averror = averror;
}
