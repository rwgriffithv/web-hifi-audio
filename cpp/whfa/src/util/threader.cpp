/**
 * @file util/threader.cpp
 * @author Robert Griffith
 */
#include "util/threader.h"

namespace whfa::util
{

    /**
     * whfa::util::Threader::StateHandler public methods
     */

    Threader::StateHandler::~StateHandler()
    {
    }

    /**
     * whfa::util::Threader public methods
     */

    Threader::~Threader()
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _terminate = true;
        }
        _cond.notify_one();
        _thread.join();
    }

    void Threader::start(StateHandler *handler)
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _handler = handler;
            set_state_start();
        }
        _cond.notify_one();
    }

    void Threader::stop()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        set_state_stop();
    }

    void Threader::pause()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        set_state_pause();
    }

    void Threader::get_state(State &state)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        state = _state;
    }

    /**
     * whfa::util::Threader protected methods
     */

    Threader::Threader()
        : _state({.run = false,
                  .error = 0,
                  .timestamp = 0}),
          _handler(nullptr),
          _terminate(false),
          _thread(&Threader::execute_loop, this)
    {
    }

    void Threader::execute_loop()
    {
        do
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cond.wait(lock, [=]
                       { return _state.run || _terminate; });
            // entire loop body remains locked
            if (_terminate)
            {
                break;
            }

            execute_loop_body();

            lock.unlock();
        } while (true);
    }

    const Threader::State &Threader::get_state()
    {
        return _state;
    }

    void Threader::set_state_start()
    {
        _state.run = true;
        _state.error = 0;
        _state.timestamp = 0;
        if (_handler != nullptr)
        {
            _handler->handle(_state);
        }
    }

    void Threader::set_state_stop(int error)
    {
        _state.run = false;
        _state.timestamp = 0;
        _state.error = error;
        if (_handler != nullptr)
        {
            _handler->handle(_state);
        }
    }

    void Threader::set_state_pause(int error)
    {
        _state.run = false;
        _state.error = error;
        if (_handler != nullptr)
        {
            _handler->handle(_state);
        }
    }

    void Threader::set_state_error(int error)
    {
        _state.error = error;
        if (_handler != nullptr)
        {
            _handler->handle(_state);
        }
    }

    void Threader::set_state_timestamp(int64_t timestamp)
    {
        _state.timestamp = timestamp;
    }

}