/**
 * @file test/pcm.cpp
 * @author Robert Griffith
 */
#include "test/pcm.h"

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

    /// @brief base of output test filenames
    constexpr const char *__TESTFILENAMEBASE = "test_output";

    /// @brief convenience alias for thread state
    using WUTState = wu::Threader::State;
    /// @brief convenience alias for thread state handler
    using WUTStateHandler = wu::Threader::StateHandler;

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

    /// @brief local pcm context to reuse
    wp::Context __c;
    /// @brief local pcm reader to reuse
    wp::Reader __r(__c);
    /// @brief local pcm decoder to reuse
    wp::Decoder __d(__c);
    /// @brief local pcm player to reuse
    wp::Player __p(__c);
    /// @brief local pcm writer to reuse
    wp::Writer __w(__c);

    /// @brief local mutex to synchronize local object usage
    std::mutex __mtx;
    /// @brief local condition variable handle notifications
    std::condition_variable __cond;

    /**
     * @brief initialize static pcm context (libav configuration)
     */
    void init_context()
    {
        static bool init = false;
        if (init)
        {
            std::cout << "libav formats & networking already initialized" << std::endl;
        }
        else
        {
            std::cout << "initializing libav formats & networking" << std::endl;
            wp::Context::register_formats();
            wp::Context::enable_networking();
            init = true;
        }
    }

    /**
     * @brief test pcm writing of specified output type
     *
     * @param url url to file to read
     * @param ot output type for written file
     */
    void test_write(const char *url, wp::Writer::OutputType ot)
    {
        init_context();
        wu::Threader::State state;
        BaseSH r_sh(__c, "Reader");
        BaseSH d_sh(__c, "Decoder");
        NotifierSH p_sh(__c, __cond, "Player");
        NotifierSH w_sh(__c, __cond, "Writer");
        int rv;

        std::cout << "openining input: " << url << std::endl;
        rv = __c.open(url);
        if (rv != 0)
        {
            std::cerr << "failed to open input: " << url << std::endl;
            wu::print_error(rv);
            return;
        }
        std::string ofname = __TESTFILENAMEBASE;
        switch (ot)
        {
        case wp::Writer::OutputType::FILE_RAW:
            ofname.append(".raw");
            break;
        case wp::Writer::OutputType::FILE_WAV:
            ofname.append(".wav");
            break;
        }
        std::cout << "opening output file: " << ofname << std::endl;
        if (!__w.open(ofname.c_str(), ot))
        {
            std::cerr << "failed to open output file: " << ofname << std::endl;
            __w.get_state(state);
            wu::print_error(state.error);
            return;
        }

        std::cout << "starting writer thread" << std::endl;
        __w.start(&p_sh);
        std::cout << "starting decoder thread" << std::endl;
        __d.start(&d_sh);
        std::cout << "starting reader thread" << std::endl;
        __r.start(&r_sh);
    }
}

namespace whfa::test
{

    void test_play(const char *url, const char *dev)
    {
        std::unique_lock<std::mutex> lk(__mtx);
        std::cout << "TESTING " << __func__ << std::endl;

        init_context();
        wu::Threader::State state;
        BaseSH r_sh(__c, "Reader");
        BaseSH d_sh(__c, "Decoder");
        NotifierSH p_sh(__c, __cond, "Player");
        NotifierSH w_sh(__c, __cond, "Writer");
        int rv;

        std::cout << "openining input: " << url << std::endl;
        rv = __c.open(url);
        if (rv != 0)
        {
            std::cerr << "failed to open input: " << url << std::endl;
            wu::print_error(rv);
            return;
        }

        std::cout << "opening output device: " << dev << std::endl;
        if (!__p.open(dev))
        {
            std::cerr << "failed to open output device: " << dev << std::endl;
            __p.get_state(state);
            wu::print_error(state.error);
            return;
        }
        __p.configure(); // using default resample & latency

        std::cout << "starting player thread" << std::endl;
        __p.start(&p_sh);
        std::cout << "starting decoder thread" << std::endl;
        __d.start(&d_sh);
        std::cout << "starting reader thread" << std::endl;
        __r.start(&r_sh);

        std::cout << "waiting to finish..." << std::endl;
        __cond.wait(lk);
        std::cout << "DONE with " << __func__ << std::endl;
    }

    void test_write_raw(const char *url)
    {
        std::unique_lock<std::mutex> lk(__mtx);
        std::cout << "TESTING " << __func__ << std::endl;

        test_write(url, wp::Writer::OutputType::FILE_RAW);

        std::cout << "waiting to finish..." << std::endl;
        __cond.wait(lk);
        std::cout << "DONE with " << __func__ << std::endl;
    }

    void test_write_wav(const char *url)
    {
        std::unique_lock<std::mutex> lk(__mtx);
        std::cout << "TESTING " << __func__ << std::endl;

        test_write(url, wp::Writer::OutputType::FILE_WAV);

        std::cout << "waiting to finish..." << std::endl;
        __cond.wait(lk);
        std::cout << "DONE with " << __func__ << std::endl;
    }

}