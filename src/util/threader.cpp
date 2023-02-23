/**
 * @file threader.cpp
 * @author Robert Griffith
 */
#include "threader.h"

/**
 * util::Threader public methods
 */

util::Threader::~Threader()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _state.terminate = true;
    lock.unlock();
    _cond.notify_one();
    _thread.join();
}

void util::Threader::start(StateCallback callback)
{
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _callback = callback;
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

/**
 * util::Threader protected methods
 */

util::Threader::Threader()
    : _state({.run = false,
              .terminate = false,
              .error = 0,
              .timestamp = 0}),
      _callback(nullptr),
      _thread(&Threader::execute_loop, this)
{
}

void util::Threader::execute_loop()
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

        execute_loop_body();

        lock.unlock();
    } while (true);
}

const util::Threader::State &util::Threader::get_state()
{
    return _state;
}

void util::Threader::set_state_start()
{
    _state.run = true;
    _state.error = 0;
    _state.timestamp = 0;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_stop()
{
    _state.run = false;
    _state.timestamp = 0;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_stop(int error)
{
    _state.run = false;
    _state.timestamp = 0;
    _state.error = error;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_pause()
{
    _state.run = false;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_pause(int error)
{
    _state.run = false;
    _state.error = error;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_error(int error)
{
    _state.error = error;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}

void util::Threader::set_state_timestamp(int64_t timestamp)
{
    _state.timestamp = timestamp;
    if (_callback != nullptr)
    {
        _callback(_state);
    }
}
