/**
 * @file writer.h
 * @author Robert Griffith
 */
#pragma once

#include "pcm/context.h"

#include <alsa/asoundlib.h>

#include <fstream>

namespace whfa::pcm
{

    /**
     * @class whfa::pcm::Writer
     * @brief class for parallel writing of PCM audio frames
     *
     * context worker class to abstract forwarding libav frames to files and devices
     * will write to only one sink at a time (potentially changed later)
     * a context should only have one writer, as it consumes frames destructively from the queue
     *
     * @todo: utilize queue pop timeouts? period determined from sample rate?
     * @todo: try doing multiple OutputTypes at once (could make it a bitfield to OR together)
     */
    class Writer : public Context::Worker
    {
    public:
        /// @brief function type to handle writing to device
        using DeviceWriter = int (*)(snd_pcm_t *, const AVFrame &, const Context::StreamSpec &, void **);
        /// @brief function type to handle writing to file
        using FileWriter = int (*)(std::ofstream &, const AVFrame &, const Context::StreamSpec &);

        /**
         * @enum whfa::Writer::OutputType
         * @brief enum defining the output destination
         */
        enum OutputType
        {
            /// @brief sound card device output
            DEVICE,
            /// @brief PCM file output
            FILE_RAW,
            /// @brief PCM WAV file output
            FILE_WAV
        };

        /**
         * @brief constructor
         *
         * @param context threadsafe audio context to access
         */
        Writer(Context &context);

        /**
         * @brief destructor to ensure file closure
         */
        ~Writer();

        /**
         * @brief open connection to output destiation to write to
         *
         * sets mode, open connection, sets spec, and sets writer
         *
         * @param name the file or device name to use
         * @param mode the specified mode of output / writing
         * @return true on success, false otherwise
         */
        bool open(const char *name, OutputType mode);

        /**
         * @brief close open output destination(s)
         *
         * @return true on success, false otherwise
         */
        bool close();

    protected:
        /**
         * @brief write queued frames to opened destination
         *
         * upon failure, pauses and sets error state without altering context or closing
         */
        void execute_loop_body() override;

        /**
         * @brief not threadsafe close open output destination(s)
         */
        bool close_unsafe();

        /// @brief mode of output / writing
        OutputType _mode;
        /// @brief alsa PCM device handle
        snd_pcm_t *_dev;
        /// @brief output file stream, closed if invalid
        std::ofstream _ofs;
        /// @brief context stream specification
        Context::StreamSpec _spec;
        /// @brief planar data pointer buffer
        void **_pbuf;
        /// @brief function to write to device with
        DeviceWriter _dev_wfn;
        /// @brief function to write to file with
        FileWriter _file_wfn;
    };

}