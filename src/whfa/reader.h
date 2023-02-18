/*
class for parallel reading of packets from an audio stream into a queue

@author Robert Griffith
*/
#pragma once

#include "threader.h"

namespace whfa
{

    class Reader : public Threader
    {
    public:
        Reader(whfa::Context &context);

        /*
        @param pos_pts presentation timestamp in frames using AV_TIME_BASE fps
        */
        bool seek(int64_t pos_pts);

    protected:
        void thread_loop_body() override;
    };

}