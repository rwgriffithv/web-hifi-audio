/**
 * @file test/main.cpp
 * @author Robert Griffith
 */
#include "test/net.h"
#include "test/pcm.h"

#include <cstring>
#include <iostream>
#include <vector>

namespace wt = whfa::test;

namespace
{

    /**
     * @brief print CLI usage
     */
    void print_usage()
    {
        std::cout << "\
usage:\n\
   <application> <input url> -d <output device name> -p <port>\n\
\n\
flags specify options that specify optional tests\n\
\n\
-d <output device name>:\n\
    test playback on specified device\n\
-p <port>\n\
    test networked capabilities (end-to-end) using specified port\n\
\n";
    }

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
    // must be at least two, and even
    if (argc < 2 || argc & 1)
    {
        print_usage();
        return 1;
    }

    const char *url = argv[1];
    std::vector<const char *> devs;
    std::vector<const char *> ports;
    bool flagerr = false;
    for (int i = 2; i < argc; i += 2)
    {
        const char *flag = argv[i];
        if (strcmp(flag, "-d") == 0)
        {
            devs.push_back(argv[i + 1]);
        }
        else if (strcmp(flag, "-p") == 0)
        {
            ports.push_back(argv[i + 1]);
        }
        else
        {
            std::cerr << "ignoring unrecognized argument: " << flag << std::endl;
            flagerr = true;
        }
    }
    if (flagerr)
    {
        print_usage();
    }

    // test net
    for (const char *p_s : ports)
    {
        const long p_l = std::strtol(p_s, nullptr, 0);
        if (p_l < 0 || p_l > 0xFFFF)
        {
            std::cerr << "WARNING: port string " << p_s << "was outside of valid 16-bit range" << std::endl;
        }
        const uint16_t p = static_cast<uint16_t>(p_l & 0xFFFF);
        std::cout << "testing net functions with port: " << p << std::endl;
        wt::test_tcpconnection(p);
        wt::test_tcpramfile(p);
    }

    // test pcm
    std::cout << "testing base pcm functionality with url: " << url << std::endl;
    wt::test_write_raw(url);
    wt::test_write_wav(url);
    for (const char *d : devs)
    {
        std::cout << "testing play function with device: " << d << std::endl;
        wt::test_play(url, d);
    }

    return 0;
}