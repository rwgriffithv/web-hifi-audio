/*
@author Robert Griffith
*/

#include "context.h"

namespace
{
    int flush_format(AVFormatContext *format)
    {
        if (format == nullptr)
        {
            return 0;
        }
        int rv = 0;
        // TODO: flush
        return rv;
    }

    int flush_codec(AVCodecContext *codec)
    {
        if (codec == nullptr)
        {
            return 0;
        }
        int rv = 0;
        // TODO: flush
        return rv;
    }

    int free_format(AVFormatContext **format)
    {
        if (format == nullptr)
        {
            return 0;
        }
        flush_format(*format);
        int rv = 0;
        // TODO: close and free, always set format to nullptr
        *format = nullptr;
        return rv;
    }

    int free_codec(AVCodecContext **codec)
    {
        if (codec == nullptr)
        {
            return 0;
        }
        flush_codec(*codec);
        int rv = 0;
        // TODO: close and free, always set format to nullptr
        *codec = nullptr;
        return rv;
    }

    void free_packet(AVPacket *&&packet)
    {
        av_packet_free(&packet);
    }

    void free_frame(AVFrame *&&frame)
    {
        av_frame_free(&frame);
    }
}

/*
static public methods
*/

void whfa::Context::register_formats()
{
    av_register_all();
}

void whfa::Context::enable_networking()
{
    avformat_network_init();
}

void whfa::Context::disable_networking()
{
    avformat_network_deinit();
}

/*
public methods
*/

whfa::Context::Context(size_t packet_queue_capacity, size_t frame_queue_capacity)
    : m_fmt_ctxt(nullptr),
      m_cdc_ctxt(nullptr),
      m_stream_idx(-1),
      m_packets(packet_queue_capacity),
      m_frames(frame_queue_capacity)
{
}

whfa::Context::~Context()
{
    close();
}

int whfa::Context::open(const char *url)
{
    /*
    not using get_format() or get_codec()
    need to lock regardless of whether format or codec are valid
    */
    std::lock_guard<std::mutex> f_lg(m_fmt_mtx);
    std::lock_guard<std::mutex> c_lg(m_cdc_mtx);

    if (m_cdc_ctxt != nullptr)
    {
        free_codec(&m_cdc_ctxt);
    }
    if (m_fmt_ctxt != nullptr)
    {
        free_format(&m_fmt_ctxt);
    }
    m_stream_idx = -1;

    int rv;
    rv = !avformat_open_input(&m_fmt_ctxt, url, nullptr, nullptr);
    if (rv != 0)
    {
        return rv;
    }
    rv = avformat_find_stream_info(m_fmt_ctxt, nullptr);
    if (rv < 0)
    {
        return rv;
    }
    AVCodec *codec;
    rv = av_find_best_stream(m_fmt_ctxt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (rv < 0)
    {
        return rv;
    }
    m_stream_idx = rv;
    AVCodecParameters *params = m_fmt_ctxt->streams[m_stream_idx]->codecpar;
    m_cdc_ctxt = avcodec_alloc_context3(codec);
    rv = avcodec_parameters_to_context(m_cdc_ctxt, params);
    if (rv < 0)
    {
        return rv;
    }
    rv = avcodec_open2(m_cdc_ctxt, codec, nullptr);
    if (rv != 0)
    {
        return rv;
    }
    return 0;
}

int whfa::Context::close()
{
    AVFormatContext *f_c;
    AVCodecContext *c_c;
    std::mutex *f_l = get_format(&f_c, nullptr);
    std::mutex *c_l = get_codec(&c_c);
    int rv = 0;
    rv |= free_codec(&c_c);
    rv |= free_format(&f_c);
    if (c_l != nullptr)
    {
        c_l->unlock();
    }
    if (f_l != nullptr)
    {
        m_stream_idx = -1;
        f_l->unlock();
    }
    return rv;
}

int whfa::Context::flush()
{
    AVFormatContext *f_c;
    AVCodecContext *c_c;
    std::mutex *f_l = get_format(&f_c, nullptr);
    std::mutex *c_l = get_codec(&c_c);
    int rv = 0;
    rv |= flush_codec(c_c);
    rv |= flush_format(f_c);
    if (c_l != nullptr)
    {
        c_l->unlock();
    }
    if (f_l != nullptr)
    {
        m_packets.flush(free_packet);
        m_frames.flush(free_frame);
        f_l->unlock();
    }
    return rv;
}

std::mutex *whfa::Context::get_format(AVFormatContext **format, int *stream_idx)
{
    if (format == nullptr)
    {
        return nullptr;
    }
    m_fmt_mtx.lock();
    *format = m_fmt_ctxt;
    if (m_fmt_ctxt == nullptr)
    {
        m_fmt_mtx.unlock();
        return nullptr;
    }
    if (stream_idx != nullptr)
    {
        *stream_idx = m_stream_idx;
    }
    return &m_fmt_mtx;
}

std::mutex *whfa::Context::get_codec(AVCodecContext **codec)
{
    if (codec == nullptr)
    {
        return nullptr;
    }
    m_cdc_mtx.lock();
    *codec = m_cdc_ctxt;
    if (m_cdc_ctxt == nullptr)
    {
        m_cdc_mtx.unlock();
        return nullptr;
    }
    return &m_cdc_mtx;
}

util::DualBlockingQueue<AVPacket *> *whfa::Context::get_packet_queue()
{
    return &m_packets;
}

util::DualBlockingQueue<AVFrame *> *whfa::Context::get_frame_queue()
{
    return &m_frames;
}

bool whfa::Context::get_sample_spec(AVSampleFormat &format, int &channels, int &rate)
{
    AVCodecContext *c_c;
    std::mutex *c_l = get_codec(&c_c);
    if (c_l == nullptr)
    {
        return false;
    }
    format = c_c->sample_fmt;
    rate = c_c->sample_rate;
    channels = c_c->channels;
    c_l->unlock();
    return true;
}
