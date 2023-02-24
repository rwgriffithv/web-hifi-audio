/**
 * @file main.cpp
 * @author Robert Griffith
 *
 * @todo implement cli and test methods
 * @todo building on WSL still has linking errors, makefile was hacked together to just find compile-time errors better
 */
#include "whfa/decoder.h"
#include "whfa/reader.h"
#include "whfa/writer.h"

#include <condition_variable>
#include <iostream>
#include <fstream>
#include <mutex>

namespace
{
    constexpr size_t __CAP_QUEUE_P = 500;
    constexpr size_t __CAP_QUEUE_F = 500;

    constexpr size_t __ERRBUFSZ = 256;

    constexpr const char *__OUTDEV = "default";
    constexpr const char *__OUTWAV = "out.wav";
    constexpr const char *__OUTRAW = "out.raw";

    char __errbuf[__ERRBUFSZ];
    whfa::Context *__ctxt;
    std::mutex __mtx;
    std::condition_variable __cond;

    void print_error(int averror)
    {
        if (av_strerror(averror, __errbuf, __ERRBUFSZ) != 0)
        {
            snprintf(__errbuf, __ERRBUFSZ, "no error description found");
        }
        std::cerr << "AVERROR (" << averror << ") : " << __errbuf << std::endl;
    }

    void set_context(whfa::Context &c)
    {
        __ctxt = &c;
    }

    void state_cb(const util::Threader::State &s, const char *str)
    {
        if (s.error != 0)
        {
            std::cerr << "CALLBACK (" << str << ")" << std::endl;
            print_error(s.error);
            std::cerr << "TIMESTAMP: " << s.timestamp << std::endl;
            if (!s.run)
            {
                __ctxt->close();
            }
        }
    }

    void state_cb_r(const util::Threader::State &s)
    {
        state_cb(s, "Reader");
    }

    void state_cb_d(const util::Threader::State &s)
    {
        state_cb(s, "Decoder");
    }

    void state_cb_w(const util::Threader::State &s)
    {
        state_cb(s, "Writer");
        if (!s.run)
        {
            __cond.notify_one();
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    whfa::Context c(__CAP_QUEUE_F, __CAP_QUEUE_P);
    set_context(c);
    
    whfa::Reader r(c);
    whfa::Decoder d(c);
    whfa::Writer w(c);
    
    util::Threader::State state;
    int rv;

    std::cout << "initializing libav formats & networking" << std::endl;
    whfa::Context::register_formats();
    whfa::Context::enable_networking();

    std::cout << "openining " << argv[1] << std::endl;

    rv = c.open(argv[1]);
    if (rv != 0)
    {
        std::cerr << "failed to open input" << std::endl;
        print_error(rv);
        return 1;
    }

    if (!w.open(__OUTWAV, whfa::Writer::OutputType::FILE_RAW))
    {
        std::cerr << "failed to open output" << std::endl;
        w.get_state(state);
        print_error(state.error);
        return 1;
    }


    w.start(state_cb_w);
    d.start(state_cb_d);
    r.start(state_cb_r);

    std::unique_lock<std::mutex> lk(__mtx);
    __cond.wait(lk);

    std::cout << "DONE: no longer waiting" << std::endl;

    // all threaders join threads in destructor
    // context and writer both close in destructor

    return 0;
}