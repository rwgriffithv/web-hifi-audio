/*
class for parallel writing of audio frames in PCM
can be dynamically configured to write to either files or sound cards

TODO: write as raw PCM, wav, or flac to file
TODO: write raw PCM to sound card using ALSA
TODO: determine queue pop timeouts from SampleSpec rate
TODO: maybe (probably) refactor to get samplespec

@author Robert Griffith
*/
#pragma once

#include "context.h"

namespace whfa
{

    class Writer : public virtual Context::Worker
    {
    public:
        Writer(Context &context);

        bool write_to_file(const char *filename, const Context::SampleSpec &spec);

        bool write_to_device(const char *devname, const Context::SampleSpec &spec);

    protected:
        void thread_loop_body() override;
    };

}