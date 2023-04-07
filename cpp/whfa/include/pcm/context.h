/**
 * @file pcm/context.h
 * @author Robert Griffith
 */
#pragma once

#include "util/dbpqueue.h"
#include "util/threader.h"

extern "C"
{
#include <libavformat/avformat.h>
}

namespace whfa::pcm
{

    /**
     * @class whfa::pcm::Context
     * @brief threadsafe class for synchronized shared context of one audio stream
     *
     * provides synchronized access to libav audio context objects and packet and frame queues
     * designed for demuxing one audio stream and facilitating parallel operation
     */
    class Context
    {
    public:
        /// @brief default packet queue capacity
        static constexpr size_t DEF_PKT_QCAP = 1024;
        /// @brief default frame queue capacity
        static constexpr size_t DEF_FRM_QCAP = 1024;

        /**
         * @class whfa::pcm::Context::Worker
         * @brief simple base Threader class for working with Context
         */
        class Worker : public util::Threader
        {
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
         * @struct whfa::pcm::Context::StreamSpec
         * @brief struct for holding stream information
         *
         * bit-width = size of container for each sample
         * bit-depth = number of bits in container used for each sample
         */
        struct StreamSpec
        {
            /// @brief sample format (includes bit-width & planar/interleaved)
            AVSampleFormat format;
            /// @brief units of duration, use av_rescale_q for conversions
            AVRational timebase;
            /// @brief total duration in time base units
            int64_t duration;
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

         * @param pkt_qcap capacity of underlying packet DBPQueue
         * @param frm_qcap capacity of underlying frame DBPQueue
         */
        Context(size_t pkt_qcap = DEF_PKT_QCAP, size_t frm_qcap = DEF_FRM_QCAP);

        /**
         * @brief destructor, closes context
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
         * @brief get stream specification for currently opened format & codec
         *
         * @param[out] spec stream specification
         * @return false if format & codec are invalid
         */
        bool get_stream_spec(StreamSpec &spec);

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