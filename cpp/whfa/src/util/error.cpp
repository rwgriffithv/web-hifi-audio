/**
 * @file util/error.cpp
 * @author Robert Griffith
 */
#include "util/error.h"

extern "C"
{
#include <libavutil/error.h>
}

#include <iostream>

namespace
{

    /// @brief size of error string buffer
    constexpr size_t __ERRBUFSZ = 256;

}

namespace whfa::util
{

    void print_error(int error)
    {
        stream_error(std::cerr, error);
    }

    void stream_error(std::ostream &os, int error)
    {
        char __errbuf[__ERRBUFSZ];
        if (av_strerror(error, __errbuf, __ERRBUFSZ) != 0)
        {
            switch (error)
            {
            case ENONE:
                snprintf(__errbuf, __ERRBUFSZ, "no error");
                break;
            case ENET_CONNFAIL:
                snprintf(__errbuf, __ERRBUFSZ, "connection failed to establish");
                break;
            case ENET_TXFAIL:
                snprintf(__errbuf, __ERRBUFSZ, "tranmit failed");
                break;
            case EPCM_CODECINVAL:
                snprintf(__errbuf, __ERRBUFSZ, "invalid libav codec context");
                break;
            case EPCM_FORMATINVAL:
                snprintf(__errbuf, __ERRBUFSZ, "invalid libav format context");
                break;
            case EPCM_CODECINVAL | EPCM_FORMATINVAL:
                snprintf(__errbuf, __ERRBUFSZ, "invalid libav format & codec context");
            default:
                snprintf(__errbuf, __ERRBUFSZ, "no error description found");
            }
        }
        os << "ERROR (" << error << ") : " << __errbuf << std::endl;
    }

}