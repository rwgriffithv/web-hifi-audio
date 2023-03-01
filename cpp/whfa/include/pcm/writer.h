/**
 * @file writer.h
 * @author Robert Griffith
 */
#pragma once

#include "pcm/context.h"

#include <fstream>

namespace whfa::pcm
{

    /**
     * @class whfa::pcm::Writer
     * @brief class for parallel writing of PCM audio frames
     *
     * context worker class to abstract forwarding libav frames to files
     * will write to only one sink at a time (potentially changed later)
     * a context should only have one writer/player, as it consumes frames destructively from the queue
     *
     * @todo: try creating common base class for Player and Writer, multithreaded processing of each poppped frame
     */
    class Writer : public Context::Worker
    {
    public:
        /**
         * @class whfa::pcm::Writer::FileWriter
         * @brief small class to handle efficient varitations of writing to file
         */
        class FileWriter
        {
        public:
            /**
             * @brief constructor
             *
             * @param ofs open output file stream to write to
             * @param spec context stream specification
             */
            FileWriter(std::ofstream &ofs, const Context::StreamSpec &spec);

            /**
             * @brief destructor
             */
            virtual ~FileWriter();

            /**
             * @brief write frame to device
             *
             * @param frame libav frame to write
             * @return 0 on success, error code on failure
             */
            virtual int write(const AVFrame &frame) = 0;

        protected:
            /// @brief output file stream
            std::ofstream *_ofs;
            /// @brief bytewidth of individual channel sample
            const int _bw;
        };

        /**
         * @enum whfa::Writer::OutputType
         * @brief enum defining the output destination
         */
        enum OutputType
        {
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
         * @param filepath the file location to open
         * @param mode the specified mode of output / writing
         * @return true on success, false otherwise
         */
        bool open(const char *filepath, OutputType mode);

        /**
         * @brief close open output destination(s)
         */
        void close();

    protected:
        /**
         * @brief write queued frames to opened destination
         *
         * upon failure, pauses and sets error state without altering context or closing
         */
        void execute_loop_body() override;

        /// @brief mode of output / writing
        OutputType _mode;
        /// @brief output file stream, closed if invalid
        std::ofstream _ofs;
        /// @brief context stream specification
        Context::StreamSpec _spec;
        /// @brief class to write to file with
        FileWriter *_writer;
    };

}