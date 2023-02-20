/*
class for parallel decoding of packets from a queue into a frame queue

@author Robert Griffith
*/
#pragma once

#include "context.h"

namespace whfa
{

    class Decoder : public virtual Context::Worker
    {
    public:
        Decoder(Context &context);

    protected:
        void thread_loop_body() override;
    };

}
