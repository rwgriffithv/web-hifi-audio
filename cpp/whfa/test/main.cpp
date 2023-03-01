/**
 * @file main.cpp
 * @author Robert Griffith
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

    constexpr size_t __ERRBUFSZ = 256;

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

    void print_usage()
    {
        std::cout << "\
usage:\n\
   <application> <input url> -play <output device name>\n\
   <application> <input url> -raw <output file name>\n\
   <application> <input url> -wav <output file name>\n";
    }

    void set_context(wp::Context &c)
    {
        __ctxt = &c;
    }

    void state_cb(const wu::Threader::State &s, const char *str)
    {
        std::cerr << "CALLBACK (" << str << ")" << std::endl;
        std::cerr << "TIMESTAMP: " << s.timestamp << std::endl;
        if (s.error != 0)
        {
            print_error(s.error);
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
    if (argc != 4)
    {
        print_usage();
        return 1;
    }

    wp::Context c;
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
        std::cerr << "failed to open input: " << argv[1] << std::endl;
        print_error(rv);
        return 1;
    }

    std::cout << "parsing option: " << argv[2] << std::endl;
    if (strcmp(argv[2], "-play") == 0)
    {
        // playing to device
        if (!p.open(argv[3]))
        {
            std::cerr << "failed to open device output: " << argv[3] << std::endl;
            p.get_state(state);
            print_error(state.error);
            return 1;
        }
        p.configure(); // using default resample & latency
        p.start(state_cb_p);
    }
    else
    {
        wp::Writer::OutputType ot;
        bool valid = false;
        if (strcmp(argv[2], "-raw") == 0)
        {
            ot = wp::Writer::OutputType::FILE_RAW;
            valid = true;
        }
        else if (strcmp(argv[2], "-wav") == 0)
        {
            ot = wp::Writer::OutputType::FILE_WAV;
            valid = true;
        }
        if (!valid)
        {
            std::cerr << "invalid option: " << argv[2] << std::endl;
            print_usage();
            return 1;
        }
        // writing to file
        if (!w.open(argv[3], ot))
        {
            std::cerr << "failed to open file output: " << argv[3] << std::endl;
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