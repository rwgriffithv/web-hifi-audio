/**
 * @file pcm/decoder.h
 * @author Robert Griffith
 */
#pragma once

#include "pcm/context.h"

namespace whfa::pcm
{

    /**
     * @class whfa::pcm::Decoder
     * @brief class for parallel decoding of packets from queue into a frame queue
     *
     * context worker class to abstract decoding packets using libav
     */
    class Decoder : public Context::Worker
    {
    public:
        /**
         * @brief constructor
         *
         * @param context threadsafe audio context to access
         */
        Decoder(Context &context);

    protected:
        /**
         * @brief decode queued packets and enqueue decoded frames
         *
         * attempts to decode one packet per iteration, can enqueue multiple frames
         * upon failure, pauses and sets error state without altering context
         */
        void execute_loop_body() override;
    };

}
