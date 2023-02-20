#include "writer.h"

whfa::Writer::Writer(whfa::Context &context)
    : Worker(context)
{
}

bool whfa::Writer::write_to_file(const char *filename, const Context::SampleSpec &spec)
{
    // TODO: take configuration from ../main.cpp
    return false;
}

bool whfa::Writer::write_to_device(const char *devname, const Context::SampleSpec &spec)
{
    // TODO: take configuration bits from ../main.cpp and add ALSA hw configuration
    return false;
}

void whfa::Writer::thread_loop_body()
{
    // TODO: take file I/O work from ../main.cpp and add ALSA work
}
