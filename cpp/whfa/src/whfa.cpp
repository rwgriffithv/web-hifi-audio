/**
 * @file whfa.cpp
 * @author Robert Griffith
 *
 * @todo load config file, communicate w/ app clients, use ramfiles
 */
#include "pcm/decoder.h"
#include "pcm/player.h"
#include "pcm/reader.h"
#include "pcm/writer.h"
#include "util/error.h"

#include <condition_variable>
#include <iostream>
#include <fstream>
#include <mutex>

namespace wp = whfa::pcm;
namespace wu = whfa::util;

namespace
{

    /// @brief convenience alias for thread state
    using WUTState = wu::Threader::State;
    /// @brief convenience alias for thread state handler
    using WUTStateHandler = wu::Threader::StateHandler;

    /**
     * @brief print CLI usage
     */
    void print_usage()
    {
        std::cout << "\
usage:\n\
   <application> <input url> -play <output device name>\n\
   <application> <input url> -raw <output file name>\n\
   <application> <input url> -wav <output file name>\n\
\n";
    }

    /**
     * @class BaseSH
     * @brief class for simple threader state handling
     */
    class BaseSH : public WUTStateHandler
    {
    public:
        /**
         * @brief constructor
         *
         * @param c pcm context
         * @param name name of state handler to use in stderr statements
         */
        BaseSH(wp::Context &c, const char *name)
            : _c(&c),
              _name(name)
        {
        }

        /**
         * @brief handle changed state
         *
         * @param s state
         */
        void handle(const WUTState &s) override
        {
            std::cerr << "CALLBACK (" << _name << ")" << std::endl;
            std::cerr << "TIMESTAMP: " << s.timestamp << std::endl;
            if (s.error != 0)
            {
                wu::print_error(s.error);
                if (!s.run)
                {
                    _c->close();
                }
            }
        }

    protected:
        /// @brief pcm context
        wp::Context *_c;
        /// @brief name of state handler to use in stderr statements
        const char *_name;
    };

    class NotifierSH : public BaseSH
    {
    public:
        /**
         * @brief constructor
         *
         * @param c pcm context
         * @param cv condition variable to notify when no longer running
         * @param name name of state handler to use in stderr statements
         */
        NotifierSH(wp::Context &c, std::condition_variable &cv, const char *name)
            : BaseSH(c, name), _cv(&cv)
        {
        }
        /**
         * @brief handle changed state and notify if no longer running
         *
         * @param s state
         */
        void handle(const WUTState &s) override
        {
            BaseSH::handle(s);
            if (!s.run)
            {
                _cv->notify_all();
            }
        }

    protected:
        /// @brief condition variable to notify when no longer running
        std::condition_variable *_cv;
    };

}

/**
 * @brief application entry point
 *
 * @param argc number of cli arguments
 * @param argv cli arguments
 * @return 0 on success, 1 on error
 */
int main(int argc, char **argv)
{
    if (argc != 4)
    {
        print_usage();
        return 1;
    }

    std::mutex wait_mtx;
    std::condition_variable wait_cond;

    wp::Context c;

    wp::Reader r(c);
    wp::Decoder d(c);
    wp::Player p(c);
    wp::Writer w(c);

    wu::Threader::State state;
    BaseSH r_sh(c, "Reader");
    BaseSH d_sh(c, "Decoder");
    NotifierSH p_sh(c, wait_cond, "Player");
    NotifierSH w_sh(c, wait_cond, "Writer");
    int rv;

    std::cout << "initializing libav formats & networking" << std::endl;
    wp::Context::register_formats();
    wp::Context::enable_networking();

    std::cout << "openining " << argv[1] << std::endl;
    rv = c.open(argv[1]);
    if (rv != 0)
    {
        std::cerr << "failed to open input: " << argv[1] << std::endl;
        wu::print_error(rv);
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
            wu::print_error(state.error);
            return 1;
        }
        p.configure(); // using default resample & latency
        p.start(&p_sh);
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
            wu::print_error(state.error);
            return 1;
        }
        w.start(&w_sh);
    }

    d.start(&d_sh);
    r.start(&r_sh);

    std::unique_lock<std::mutex> wait_lk(wait_mtx);
    wait_cond.wait(wait_lk);

    std::cout << "DONE: no longer waiting" << std::endl;

    // all threaders join threads in destructor
    // context, player, and writer both close in destructor

    return 0;
}