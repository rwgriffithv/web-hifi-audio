/**
 * @file context.cpp
 * @author Robert Griffith
 */
#include "pcm/context.h"

namespace
{

    /**
     * @brief frees format context and sets to nullptr
     * @param[out] format format context to free and set to nullptr
     */
    inline void free_format(AVFormatContext *&format)
    {
        if (format != nullptr)
        {
            avformat_free_context(format);
            format = nullptr;
        }
    }

    /**
     * @brief frees codec context and sets to nullptr
     * @param[out] codec codec context to free and set to nullptr
     */
    inline void free_codec(AVCodecContext *&codec)
    {
        if (codec != nullptr)
        {
            avcodec_free_context(&codec);
        }
    }

    /**
     * @brief used when flushing context packet queue
     *
     * @param packet libav packet to free
     */
    void free_packet(AVPacket *packet)
    {
        av_packet_free(&packet);
    }

    /**
     * @brief used when flushing context frame queue
     *
     * @param frame libav frame to free
     */
    void free_frame(AVFrame *frame)
    {
        av_frame_free(&frame);
    }

    /**
     * @brief frees format and codec context, sets to nullptrs and invalid stream index
     * @param[out] format format context to free and set to nullptr
     * @param[out] codec codec context to free and set to nullptr
     * @param[out] stream_idx stream index to set to -1
     */
    inline void free_context(AVFormatContext *&format, AVCodecContext *&codec, int &stream_idx)
    {
        free_format(format);
        free_codec(codec);
        stream_idx = -1;
    }
}

namespace whfa::pcm
{

    /**
     * whfa::pcm::Context::Worker public methods
     */

    Context::Worker::Worker(Context &context)
        : Threader(),
          _ctxt(&context)
    {
    }

    /**
     * whfa::pcm::Context static public methods
     */

    void Context::register_formats()
    {
        av_register_all();
    }

    void Context::enable_networking()
    {
        avformat_network_init();
    }

    void Context::disable_networking()
    {
        avformat_network_deinit();
    }

    /**
     * whfa::pcm::Context public methods
     */

    Context::Context(size_t packet_queue_capacity, size_t frame_queue_capacity)
        : _fmt_ctxt(nullptr),
          _cdc_ctxt(nullptr),
          _stm_idx(-1),
          _pkt_q(packet_queue_capacity, free_packet),
          _frm_q(frame_queue_capacity, free_frame)
    {
    }

    Context::~Context()
    {
        close();
    }

    int Context::open(const char *url)
    {
        std::lock_guard<std::mutex> f_lk(_fmt_mtx);
        std::lock_guard<std::mutex> c_lk(_cdc_mtx);

        free_context(_fmt_ctxt, _cdc_ctxt, _stm_idx);
        _frm_q.flush();
        _pkt_q.flush();

        int rv;
        if ((rv = !avformat_open_input(&_fmt_ctxt, url, nullptr, nullptr)) != 0)
        {
            return rv;
        }
        if ((rv = avformat_find_stream_info(_fmt_ctxt, nullptr)) < 0)
        {
            free_format(_fmt_ctxt);
            return rv;
        }

        AVCodec *codec;
        if ((rv = av_find_best_stream(_fmt_ctxt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0)) < 0)
        {
            free_format(_fmt_ctxt);
            return rv;
        }
        _stm_idx = rv;

        AVCodecParameters *params = _fmt_ctxt->streams[_stm_idx]->codecpar;
        _cdc_ctxt = avcodec_alloc_context3(codec);
        if ((rv = avcodec_parameters_to_context(_cdc_ctxt, params)) < 0)
        {
            free_context(_fmt_ctxt, _cdc_ctxt, _stm_idx);
            return rv;
        }
        if ((rv = avcodec_open2(_cdc_ctxt, codec, nullptr)) != 0)
        {
            free_context(_fmt_ctxt, _cdc_ctxt, _stm_idx);
            return rv;
        }
        return 0;
    }

    void Context::close()
    {
        {
            std::lock_guard<std::mutex> f_lk(_fmt_mtx);
            std::lock_guard<std::mutex> c_lk(_cdc_mtx);
            free_context(_fmt_ctxt, _cdc_ctxt, _stm_idx);
        }
        _frm_q.flush();
        _pkt_q.flush();
    }

    bool Context::get_stream_spec(StreamSpec &spec)
    {
        std::lock_guard<std::mutex> f_lk(_fmt_mtx);
        std::lock_guard<std::mutex> c_lk(_cdc_mtx);
        bool rv = _fmt_ctxt != nullptr && _cdc_ctxt != nullptr;
        if (rv)
        {
            spec.format = _cdc_ctxt->sample_fmt;
            spec.timebase = _fmt_ctxt->streams[_stm_idx]->time_base;
            spec.duration = _fmt_ctxt->streams[_stm_idx]->duration;
            spec.bitdepth = _cdc_ctxt->bits_per_raw_sample;
            spec.channels = _cdc_ctxt->channels;
            spec.rate = _cdc_ctxt->sample_rate;
        }
        return rv;
    }

    std::mutex *Context::get_format(AVFormatContext *&format, int &stream_idx)
    {
        _fmt_mtx.lock();
        format = _fmt_ctxt;
        stream_idx = _stm_idx;
        if (_fmt_ctxt == nullptr)
        {
            _fmt_mtx.unlock();
            return nullptr;
        }
        return &_fmt_mtx;
    }

    std::mutex *Context::get_codec(AVCodecContext *&codec)
    {
        _cdc_mtx.lock();
        codec = _cdc_ctxt;
        if (_cdc_ctxt == nullptr)
        {
            _cdc_mtx.unlock();
            return nullptr;
        }
        return &_cdc_mtx;
    }

    util::DBPQueue<AVPacket> &Context::get_packet_queue()
    {
        return _pkt_q;
    }

    util::DBPQueue<AVFrame> &Context::get_frame_queue()
    {
        return _frm_q;
    }

}