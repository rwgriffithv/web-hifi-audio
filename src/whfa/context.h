/*
threadsafe class for storing and maniuplating underlying libav audio context objects
also hold shared threadsafe queues for processing packets and frames

@author Robert Griffith
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

    class Context
    {
    public:
        class Worker : public util::Threader
        {
        protected:
            Worker(Context &context);
            Context *_ctxt;
        };

        struct SampleSpec
        {
            AVSampleFormat format;
            int channels;
            int rate;
        };

        static void register_formats();
        static void enable_networking();
        static void disable_networking();

        Context(size_t packet_queue_capacity, size_t frame_queue_capacity);
        ~Context();

        int open(const char *url, SampleSpec &spec);

        int close();

        int flush();

        std::mutex *get_format(AVFormatContext **format, int *stream_idx);
        std::mutex *get_codec(AVCodecContext **codec);

        util::DBPQueue<AVPacket> &get_packet_queue();
        util::DBPQueue<AVFrame> &get_frame_queue();

    private:
        AVFormatContext *_fmt_ctxt;
        AVCodecContext *_cdc_ctxt;
        int _stream_idx;

        std::mutex _fmt_mtx;
        std::mutex _cdc_mtx;

        util::DBPQueue<AVPacket> _packets;
        util::DBPQueue<AVFrame> _frames;
    };

}