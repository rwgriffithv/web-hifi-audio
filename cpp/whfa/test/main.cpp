/**
 * @file main.cpp
 * @author Robert Griffith
 *
 * @todo more tests as methods
 * @todo building on WSL still has linking errors, makefile was hacked together to just find compile-time errors better
 */
#include "pcm/decoder.h"
#include "pcm/player.h"
#include "pcm/reader.h"
#include "pcm/writer.h"

#include <condition_variable>
#include <iostream>
#include <fstream>
#include <mutex>

namespace wp = whfa::pcm;
namespace wu = whfa::util;

namespace
{
    constexpr size_t __CAP_QUEUE_P = 500;
    constexpr size_t __CAP_QUEUE_F = 500;

    constexpr size_t __ERRBUFSZ = 256;

    constexpr const char *__OUTDEV = "default";
    constexpr const char *__OUTWAV = "out.wav";
    constexpr const char *__OUTRAW = "out.raw";

    constexpr const bool __PLAY = false;

    char __errbuf[__ERRBUFSZ];
    wp::Context *__ctxt;
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

    void set_context(wp::Context &c)
    {
        __ctxt = &c;
    }

    void state_cb(const wu::Threader::State &s, const char *str)
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

    void state_cb_r(const wu::Threader::State &s)
    {
        state_cb(s, "Reader");
    }

    void state_cb_d(const wu::Threader::State &s)
    {
        state_cb(s, "Decoder");
    }

    void state_cb_w(const wu::Threader::State &s)
    {
        state_cb(s, "Writer");
        if (!s.run)
        {
            __cond.notify_one();
        }
    }

    void state_cb_p(const wu::Threader::State &s)
    {
        state_cb(s, "Player");
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

    wp::Context c(__CAP_QUEUE_F, __CAP_QUEUE_P);
    set_context(c);

    wp::Reader r(c);
    wp::Decoder d(c);
    wp::Player p(c);
    wp::Writer w(c);

    wu::Threader::State state;
    int rv;

    std::cout << "initializing libav formats & networking" << std::endl;
    wp::Context::register_formats();
    wp::Context::enable_networking();

    std::cout << "openining " << argv[1] << std::endl;

    rv = c.open(argv[1]);
    if (rv != 0)
    {
        std::cerr << "failed to open input" << std::endl;
        print_error(rv);
        return 1;
    }

    if (__PLAY)
    {
        if (!p.open(__OUTDEV))
        {
            std::cerr << "failed to open device output" << std::endl;
            p.get_state(state);
            print_error(state.error);
            return 1;
        }
        p.configure();
        p.start(state_cb_p);
    }
    else
    {
        if (!w.open(__OUTWAV, wp::Writer::OutputType::FILE_RAW))
        {
            std::cerr << "failed to open file output" << std::endl;
            w.get_state(state);
            print_error(state.error);
            return 1;
        }
        w.start(state_cb_w);
    }

    d.start(state_cb_d);
    r.start(state_cb_r);

    std::unique_lock<std::mutex> lk(__mtx);
    __cond.wait(lk);

    std::cout << "DONE: no longer waiting" << std::endl;

    // all threaders join threads in destructor
    // context, player, and writer both close in destructor

    return 0;
}