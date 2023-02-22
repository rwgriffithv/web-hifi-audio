/**
 * @file context.h
 * @author Robert Griffith
 */
#pragma once

#include "../util/dbpqueue.h"
#include "../util/threader.h"

extern "C"
{
#include <libavformat/avformat.h>
}

namespace whfa
{

    /**
     * @class whfa::Context
     * @brief threadsafe class for synchronized shared context
     *
     * provides access to underlying libav audio context objects and packet and frame queues
     */
    class Context
    {
    public:
        /**
         * @class whfa::Context::Worker
         * @brief simple base Threader class for working with Context
         */
        class Worker : public util::Threader
        {
        public:
            /// @brief error code for invalid codec
            static constexpr int CODECINVAL = 0x10;
            /// @brief error code for invalid format
            static constexpr int FORMATINVAL = 0x20;

        protected:
            /**
             * @brief hidden constructor for derived classes to use
             * @param context threadsafe audio context to access
             */
            Worker(Context &context);

            /// @brief the libav audio context object
            Context *_ctxt;
        };

        /**
         * @struct whfa::Context::SampleSpec
         * @brief struct for holding sample information
         *
         * bit-width = size of container for each sample
         * bit-depth = number of bits in container used for each sample
         */
        struct SampleSpec
        {
            /// @brief sample format (includes bit-width & planar/interleaved)
            AVSampleFormat format;
            /// @brief bit-depth of each raw sample
            int bitdepth;
            /// @brief number of channels
            int channels;
            /// @brief sample frequency
            int rate;
        };

        /**
         * @brief  register all available codec formats with libav
         */
        static void register_formats();
        /**
         * @brief  enable support for opening networked streams with libav
         */
        static void enable_networking();
        /**
         * @brief  disable support for opening networked streams with libav
         */
        static void disable_networking();

        /**
         * @brief constructor

         * @param packet_queue_capacity capacity of underlying packet DBPQueue
         * @param frame_queue_capacity capacity of underlying frame DBPQueue
         */
        Context(size_t packet_queue_capacity, size_t frame_queue_capacity);
        /**
         * @brief destructor
         */
        virtual ~Context();

        /**
         * @brief open libav stream and sets format and codec context if valid audio source
         *
         * format and codec members are attempted to be freed upon error
         *
         * @param url libav stream string to source
         * @return error int, 0 on success
         */
        int open(const char *url);

        /**
         * @brief close and free format and codec contexts
         */
        void close();

        /**
         * @brief get sample specification for currently opened codec
         *
         * @param[out] spec sample specification of stream
         * @return false if codec is not opened
         */
        bool get_sample_spec(SampleSpec &spec);

        /**
         * @brief get exclusive access to format context and stream index
         *
         * lock released if format context is invalid
         * still sets format and stream_idx to copies of internal values
         *
         * @param[out] format format context
         * @param[out] stream_idx index of stream opened
         * @return pointer to locked mutex (must release), nullptr if format is invalid
         */
        std::mutex *get_format(AVFormatContext *&format, int &stream_idx);

        /**
         * @brief get exclusive access to codec context
         *
         * lock release if codec context is invalid
         * still sets codec to copy of internal value
         *
         * @param[out] codec codec context
         * @return pointer to locked mutex, nullptr if format is invalid
         */
        std::mutex *get_codec(AVCodecContext *&codec);

        /**
         * @brief get reference to threadsafe packet queue
         *
         * popped packets are the responsibility of the caller
         * packets must be freed using av_packet_free() after popping or when flushing
         *
         * @return packet queue
         */
        util::DBPQueue<AVPacket> &get_packet_queue();

        /**
         * @brief get reference to threadsafe frame queue

         * popped frames are the responsibility of the caller
         * frames must be freed using av_frame_free() after popping or when flushing
         *
         * @return frame queue
         */
        util::DBPQueue<AVFrame> &get_frame_queue();

    protected:
        /// @brief libav format context (nullptr if invalid)
        AVFormatContext *_fmt_ctxt;
        /// @brief libav codec context (nullptr if invalid)
        AVCodecContext *_cdc_ctxt;
        /// @brief stream index to audio stream in format context (-1 if invalid)
        int _stm_idx;

        /// @brief mutex synchronizing access to format context and stream index
        std::mutex _fmt_mtx;
        /// @brief mutex synchronizing access to codec context
        std::mutex _cdc_mtx;

        /// @brief threadsafe queue of pointers to libav packets on the heap
        util::DBPQueue<AVPacket> _pkt_q;
        /// @brief threadsafe queue of pointers to libav frames on the heap
        util::DBPQueue<AVFrame> _frm_q;
    };

}