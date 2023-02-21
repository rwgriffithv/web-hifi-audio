/**
 * @file writer.h
 * @author Robert Griffith
 */
#pragma once

#include "context.h"

namespace whfa
{

    /**
     * @class whfa::Writer
     * @brief class for parallel writing of audio frames in PCM
     *
     * context worker class to abstract forwarding libav frames to files and devices
     * will write to either one file or one device (restricted for performance reasons)
     * a context should only have one writer, as it consumes frames destructively from the queue
     *
     * @todo: create member objects required to facilitate open() and close()
     * @todo: write as raw PCM, wav, or flac to file (use OutputType enum)
     * @todo: write raw PCM to sound card using ALSA
     * @todo: determine queue pop timeouts from SampleSpec rate
     * @todo: try doing multiple OutputTypes at once (could make it a bitfield to OR together)
     */
    class Writer : public Context::Worker
    {
    public:
        /**
         * @enum whfa::Writer::OutputType
         * @brief enum defining the output destination
         */
        enum OutputType
        {
            /// @brief PCM file output
            FILE,
            /// @brief sound card device output
            DEVICE
        };

        /**
         * @brief constructor
         *
         * @param context threadsafe audio context to access
         */
        Writer(Context &context);

        /**
         * @brief open connection to output destiation to write to
         *
         * @param name the file or device name to use
         * @param mode the specified mode of output / writing
         *
         * @todo: take configuration from ../main.cpp and add ALSA hw configuration
         */
        bool open(const char *name, OutputType mode);

        /**
         * @brief close open output destination
         */
        bool close();

    protected:
        /**
         * @brief write queued frames to opened destination
         *
         * upon failure, pauses and sets error state without altering context or closing
         */
        void execute_loop_body() override;

        /// @brief mode of output / writing
        OutputType _mode;
    };

}