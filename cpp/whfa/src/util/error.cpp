/**
 * @file util/error.cpp
 * @author Robert Griffith
 */
#include "util/error.h"

extern "C"
{
#include <libavutil/error.h>
}

#include <cstring>
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
        char errbuf[__ERRBUFSZ];
        switch (error)
        {
        case ENONE:
            snprintf(errbuf, __ERRBUFSZ, "WHFA no error");
            break;
        case EINVCODEC:
            snprintf(errbuf, __ERRBUFSZ, "WHFA invalid libav codec context");
            break;
        case EINVFORMAT:
            snprintf(errbuf, __ERRBUFSZ, "WHFA invalid libav format context");
            break;
        case EINVSTREAM:
            snprintf(errbuf, __ERRBUFSZ, "WHFA invalid libav stream (format and/or context)");
            break;
        default:
            if (av_strerror(error, errbuf, __ERRBUFSZ) != 0)
            {
                strncpy(errbuf, strerror_r(error, errbuf, __ERRBUFSZ), __ERRBUFSZ);
            }
        }
        os << "ERROR (" << error << ") : " << errbuf << std::endl;
    }
}