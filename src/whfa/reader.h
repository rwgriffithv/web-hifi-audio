/**
 * @file reader.h
 * @author Robert Griffith


@author Robert Griffith
*/
#pragma once

#include "context.h"

namespace whfa
{

    /**
     * @class whfa::Reader
     * @brief class for parallel reading of packets from an audio stream into a queue
     *
     * context worker class to abstract reading packets using libav and seeking to new positions
     */
    class Reader : public Context::Worker
    {
    public:
        /**
         * @brief constructor
         *
         * @param context threadsafe audio context to access
         */
        Reader(Context &context);

        /**
         * @brief seek to position by timestamp
         *
         * @param pos_pts presentation timestamp in frames using AV_TIME_BASE fps
         * @return true if successful, sets error state upon failure
         */
        bool seek(int64_t pos_pts);

        /**
         * @brief seek to position by percentage
         *
         * @param pos_pct percentage of stream duration to seek to (clipped to [0,1])
         * @return true if successful, sets error state upon failure
         */
        bool seek(double pos_pct);

    protected:
        /**
         * @brief read from open context and enqueue packets
         *
         * upon failure, pauses and sets error state without altering context
         */
        void execute_loop_body() override;
    };

}