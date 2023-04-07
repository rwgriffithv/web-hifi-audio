/**
 * @file pcm/framehandler.h
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
     * @class whfa::pcm::FrameHandler
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
         * @brief handle libav frame
         *
         * @param frame libav frame to handle
         * @return 0 on success, error code on failure
         */
        virtual int handle(const AVFrame &frame) = 0;
    };

}