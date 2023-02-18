/*
threadsafe class for storing and maniuplating underlying libav audio context objects
also hold shared threadsafe queues for processing packets and frames

@author Robert Griffith
*/
#pragma once

#include "../util/dbqueue.h"

extern "C"
{
#include <libavformat/avformat.h>
}

namespace whfa
{

    class Context
    {
    public:
        static void register_formats();
        static void enable_networking();
        static void disable_networking();

        Context(size_t packet_queue_capacity, size_t frame_queue_capacity);
        ~Context();

        int open(const char *url);

        int close();

        int flush();

        std::mutex *get_format(AVFormatContext **format, int *stream_idx);
        std::mutex *get_codec(AVCodecContext **codec);

        util::DualBlockingQueue<AVPacket *> *get_packet_queue();
        util::DualBlockingQueue<AVFrame *> *get_frame_queue();

        bool get_sample_spec(AVSampleFormat &format, int &channels, int &rate);

        bool get_frames(size_t count, AVFrame **frames);

    private:
        AVFormatContext *m_fmt_ctxt;
        AVCodecContext *m_cdc_ctxt;
        int m_stream_idx;

        std::mutex m_fmt_mtx;
        std::mutex m_cdc_mtx;

        util::DualBlockingQueue<AVPacket *> m_packets;
        util::DualBlockingQueue<AVFrame *> m_frames;
    };

}