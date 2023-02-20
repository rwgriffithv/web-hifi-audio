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

    void free_packet(AVPacket *packet)
    {
        av_packet_free(&packet);
    }

    void free_frame(AVFrame *frame)
    {
        av_frame_free(&frame);
    }
}

/*
worker public methods
*/

whfa::Context::Worker::Worker(Context &context)
    : Threader(),
      _ctxt(&context)
{
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
    : _fmt_ctxt(nullptr),
      _cdc_ctxt(nullptr),
      _stream_idx(-1),
      _packets(packet_queue_capacity),
      _frames(frame_queue_capacity)
{
}

whfa::Context::~Context()
{
    close();
}

int whfa::Context::open(const char *url, SampleSpec &spec)
{
    /*
    not using get_format() or get_codec()
    need to lock regardless of whether format or codec are valid
    */
    std::lock_guard<std::mutex> f_lg(_fmt_mtx);
    std::lock_guard<std::mutex> c_lg(_cdc_mtx);

    if (_cdc_ctxt != nullptr)
    {
        free_codec(&_cdc_ctxt);
    }
    if (_fmt_ctxt != nullptr)
    {
        free_format(&_fmt_ctxt);
    }
    _stream_idx = -1;

    int rv;
    rv = !avformat_open_input(&_fmt_ctxt, url, nullptr, nullptr);
    if (rv != 0)
    {
        return rv;
    }
    rv = avformat_find_stream_info(_fmt_ctxt, nullptr);
    if (rv < 0)
    {
        return rv;
    }
    AVCodec *codec;
    rv = av_find_best_stream(_fmt_ctxt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (rv < 0)
    {
        return rv;
    }
    _stream_idx = rv;
    AVCodecParameters *params = _fmt_ctxt->streams[_stream_idx]->codecpar;
    _cdc_ctxt = avcodec_alloc_context3(codec);
    rv = avcodec_parameters_to_context(_cdc_ctxt, params);
    if (rv < 0)
    {
        return rv;
    }
    rv = avcodec_open2(_cdc_ctxt, codec, nullptr);
    if (rv != 0)
    {
        return rv;
    }

    spec.format = _cdc_ctxt->sample_fmt;
    spec.rate = _cdc_ctxt->sample_rate;
    spec.channels = _cdc_ctxt->channels;
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
        _stream_idx = -1;
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
        _packets.flush(free_packet);
        _frames.flush(free_frame);
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
    _fmt_mtx.lock();
    *format = _fmt_ctxt;
    if (_fmt_ctxt == nullptr)
    {
        _fmt_mtx.unlock();
        return nullptr;
    }
    if (stream_idx != nullptr)
    {
        *stream_idx = _stream_idx;
    }
    return &_fmt_mtx;
}

std::mutex *whfa::Context::get_codec(AVCodecContext **codec)
{
    if (codec == nullptr)
    {
        return nullptr;
    }
    _cdc_mtx.lock();
    *codec = _cdc_ctxt;
    if (_cdc_ctxt == nullptr)
    {
        _cdc_mtx.unlock();
        return nullptr;
    }
    return &_cdc_mtx;
}

util::DBPQueue<AVPacket> &whfa::Context::get_packet_queue()
{
    return _packets;
}

util::DBPQueue<AVFrame> &whfa::Context::get_frame_queue()
{
    return _frames;
}
