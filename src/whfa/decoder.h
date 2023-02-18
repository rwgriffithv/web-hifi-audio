/*
class for parallel decoding of packets from a queue into a frame queue

@author Robert Griffith
*/
#pragma once

#include "threader.h"

namespace whfa
{

    class Decoder : public Threader
    {
    public:
        Decoder(Context &context);

    protected:
        void thread_loop_body() override;
    };
}
