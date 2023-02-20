#include "threader.h"

/*
public methods
*/

util::Threader::~Threader()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _state.terminate = true;
    lock.unlock();
    _cond.notify_one();
    _thread.join();
}

void util::Threader::start()
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        set_state_start();
    }
    _cond.notify_one();
}

void util::Threader::stop()
{
    std::lock_guard<std::mutex> lock(_mtx);
    set_state_stop();
}

void util::Threader::pause()
{
    std::lock_guard<std::mutex> lock(_mtx);
    set_state_pause();
}

void util::Threader::get_state(State &state)
{
    std::lock_guard<std::mutex> lock(_mtx);
    state = _state;
}

/*
protected methods
*/

util::Threader::Threader()
    : _state({.run = false,
               .terminate = false,
               .error = 0,
               .curr_pts = 0}),
      _thread(&Threader::thread_func, this)
{
}

void util::Threader::thread_func()
{
    do
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond.wait(
            lock, [=]
            { return _state.run || _state.terminate; });
        // entire loop body remains locked
        if (_state.terminate)
        {
            break;
        }

        thread_loop_body();

        lock.unlock();
    } while (true);
}

void util::Threader::set_state_start()
{
    _state.run = true;
    _state.error = 0;
    _state.curr_pts = 0;
}

void util::Threader::set_state_stop()
{
    _state.run = false;
    _state.curr_pts = 0;
}

void util::Threader::set_state_pause()
{
    _state.run = false;
}

void util::Threader::set_state_error(int error)
{
    _state.error = error;
}