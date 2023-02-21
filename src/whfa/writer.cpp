/**
 * @file writer.cpp
 * @author Robert Griffith
 */
#include "writer.h"

/**
 * whfa::Writer public methods
 */

whfa::Writer::Writer(whfa::Context &context)
    : Worker(context),
      _mode(OutputType::FILE)
{
}

bool whfa::Writer::open(const char *name, OutputType mode)
{
    _mode = mode;
    return false;
}

bool whfa::Writer::close()
{
    return false;
}

/**
 * whfa::Writer protected methods
 */

void whfa::Writer::execute_loop_body()
{
}
