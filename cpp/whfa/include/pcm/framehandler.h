/**
 * @file framehandler.h
 * @author Robert Griffith
 */
#pragma once

extern "C"
{
#include <libavformat/avformat.h>
}

namespace whfa::pcm
{

    /**
     * @class FrameHandler
     * @brief interface for defining common frame processing (stateful function pointer alternative)
     */
    class FrameHandler
    {
    public:
        /**
         * @brief destructor
         */
        virtual ~FrameHandler();

        /**
         * @brief operate on libav frame
         *
         * @param frame libav frame to handle
         * @return 0 on success, error code on failure
         */
        virtual int handle(const AVFrame &frame) = 0;
    };

}