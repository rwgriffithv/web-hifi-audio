/*
@author Robert Griffith
*/

#include "decoder.h"

namespace
{
    void free_av_packet(AVPacket *&&packet)
    {
        av_packet_free(&packet);
    }
    void free_av_frame(AVFrame *&&frame)
    {
        av_frame_free(&frame);
    }
}

/*
static public methods
*/

void whfa::Decoder::register_formats()
{
    av_register_all();
}

void whfa::Decoder::enable_networking()
{
    avformat_network_init();
}

void whfa::Decoder::disable_networking()
{
    avformat_network_deinit();
}

/*
public methods
*/

whfa::Decoder::Decoder(size_t packet_queue_capacity, size_t frame_queue_capacity)
    : m_state({.m_ds = DecoderStatus::UNINIT,
               .m_ps = PacketStatus::UNINIT,
               .m_fs = FrameStatus::UNINIT,
               .m_d_averr = 0,
               .m_p_averr = 0,
               .m_f_averr = 0}),
      m_packet_q(packet_queue_capacity),
      m_frame_q(frame_queue_capacity),
      m_packet_t(&populate_packet_queue, this),
      m_frame_t(&populate_frame_queue, this),
      m_format_ctxt(nullptr),
      m_stream_idx(-1),
      m_codec_ctxt(nullptr)
{
}

whfa::Decoder::~Decoder()
{
    free();
}

bool whfa::Decoder::initialize(const char *url, AVSampleFormat &format, int &channels, int &sample_rate)
{
    free();

    int rv;
    rv = !avformat_open_input(&m_format_ctxt, url, nullptr, nullptr);
    if (rv != 0)
    {
        set_decoder_averr(rv);
        return false;
    }
    rv = avformat_find_stream_info(m_format_ctxt, nullptr);
    if (rv < 0)
    {
        set_decoder_averr(rv);
        return false;
    }
    AVCodec *codec;
    rv = av_find_best_stream(m_format_ctxt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (rv < 0)
    {
        set_decoder_averr(rv);
        return false;
    }
    m_stream_idx = rv;
    AVCodecParameters *params = m_format_ctxt->streams[m_stream_idx]->codecpar;
    m_codec_ctxt = avcodec_alloc_context3(codec);
    rv = avcodec_parameters_to_context(m_codec_ctxt, params);
    if (rv < 0)
    {
        set_decoder_averr(rv);
        return false;
    }
    rv = avcodec_open2(m_codec_ctxt, codec, nullptr);
    if (rv != 0)
    {
        set_decoder_averr(rv);
        return false;
    }

    m_packet_q.initialize();
    m_frame_q.initialize();

    set_initialized_state();

    format = m_codec_ctxt->sample_fmt;
    sample_rate = m_codec_ctxt->sample_rate;
    channels = m_codec_ctxt->channels;
    return true;
}

void whfa::Decoder::free()
{
    set_uninitialized_state();

    m_frame_q.free(free_av_frame);
    m_packet_q.free(free_av_packet);

    if (m_codec_ctxt != nullptr)
    {
        avcodec_free_context(&m_codec_ctxt);
        m_codec_ctxt = nullptr;
    }

    m_stream_idx = -1;

    if (m_format_ctxt != nullptr)
    {
        avformat_close_input(&m_format_ctxt);
        m_format_ctxt = nullptr;
    }
}

void whfa::Decoder::get_state(State &state)
{
    std::unique_lock<std::mutex> lock_d(m_decoder_l.m_mtx);
    state.m_ds = m_state.m_ds;
    state.m_d_averr = m_state.m_d_averr;
    std::unique_lock<std::mutex> lock_p(m_packet_l.m_mtx);
    state.m_ps = m_state.m_ps;
    state.m_p_averr = m_state.m_p_averr;
    std::unique_lock<std::mutex> lock_f(m_frame_l.m_mtx);
    state.m_fs = m_state.m_fs;
    state.m_f_averr = m_state.m_f_averr;
    lock_f.unlock();
    lock_p.unlock();
    lock_d.unlock();
}

int whfa::Decoder::get_frames(size_t count, AVFrame **frames)
{
    bool init;
    std::unique_lock<std::mutex> lock(m_decoder_l.m_mtx);
    init = m_state.m_ds == DecoderStatus::INIT;
    lock.unlock();
    if (!init)
    {
        return -1;
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (!m_frame_q.pop(frames[i]))
        {
            set_uninitialized_state();
            return i;
        }
    }
    return count;
}

/*
private methods
*/

void whfa::Decoder::populate_packet_queue()
{
    do
    {
        bool end;
        std::unique_lock<std::mutex> lock(m_packet_l.m_mtx);
        m_packet_l.m_cond.wait(
            lock, [=]
            { return m_state.m_ps == PacketStatus::READING ||
                     m_state.m_ps == PacketStatus::DESTRUCTING; });
        end = m_state.m_ps == PacketStatus::DESTRUCTING;
        lock.unlock();
        if (end)
        {
            break;
        }
        AVPacket *packet = av_packet_alloc();
        int rv;
        while ((rv = av_read_frame(m_format_ctxt, packet)) == 0)
        {
            if (packet->stream_index == m_stream_idx)
            {
                if (!m_packet_q.push(packet))
                {
                    set_packet_status(PacketStatus::UNINIT);
                }
                break;
            }
        }
        if (rv == AVERROR_EOF)
        {
            set_packet_status(PacketStatus::END_OF_FILE);
        }
        else if (rv != 0)
        {
            set_packet_averr(rv);
        }
    } while (true);
}

void whfa::Decoder::populate_frame_queue()
{
    do
    {
        bool end;
        std::unique_lock<std::mutex> lock(m_frame_l.m_mtx);
        m_frame_l.m_cond.wait(
            lock, [=]
            { return m_state.m_fs == FrameStatus::DECODING ||
                     m_state.m_fs == FrameStatus::DESTRUCTING; });
        end = m_state.m_fs == FrameStatus::DESTRUCTING;
        lock.unlock();
        if (end)
        {
            break;
        }
        AVPacket *packet;
        if (!m_packet_q.pop(packet))
        {
            set_frame_status(FrameStatus::UNINIT);
        }
        else
        {
            int rv;
            rv = avcodec_send_packet(m_codec_ctxt, packet);
            if (rv != 0)
            {
                set_frame_averr(rv);
            }
            else
            {
                AVFrame *frame = av_frame_alloc();
                while ((rv = avcodec_receive_frame(m_codec_ctxt, frame)) == 0)
                {
                    m_frame_q.push(frame);
                }
                if (rv != AVERROR(EAGAIN))
                {
                    set_frame_averr(rv);
                }
            }
        }
    } while (true);
}

void whfa::Decoder::set_uninitialized_state()
{
    std::unique_lock<std::mutex> lock_d(m_decoder_l.m_mtx);
    m_state.m_ds = DecoderStatus::UNINIT;
    std::unique_lock<std::mutex> lock_p(m_packet_l.m_mtx);
    m_state.m_ps = PacketStatus::UNINIT;
    std::unique_lock<std::mutex> lock_f(m_frame_l.m_mtx);
    m_state.m_fs = FrameStatus::UNINIT;
    lock_f.unlock();
    m_frame_l.m_cond.notify_all();
    lock_p.unlock();
    m_packet_l.m_cond.notify_all();
    lock_d.unlock();
    m_decoder_l.m_cond.notify_all();
}

void whfa::Decoder::set_initialized_state()
{
    std::unique_lock<std::mutex> lock_d(m_decoder_l.m_mtx);
    m_state.m_ds = DecoderStatus::INIT;
    std::unique_lock<std::mutex> lock_p(m_packet_l.m_mtx);
    m_state.m_ps = PacketStatus::READING;
    std::unique_lock<std::mutex> lock_f(m_frame_l.m_mtx);
    m_state.m_fs = FrameStatus::DECODING;
    lock_f.unlock();
    m_frame_l.m_cond.notify_all();
    lock_p.unlock();
    m_packet_l.m_cond.notify_all();
    lock_d.unlock();
    m_decoder_l.m_cond.notify_all();
}

void whfa::Decoder::set_decoder_status(DecoderStatus status)
{
    std::unique_lock<std::mutex> lock(m_decoder_l.m_mtx);
    m_state.m_ds = status;
    lock.unlock();
    m_decoder_l.m_cond.notify_all();
}
void whfa::Decoder::set_packet_status(PacketStatus status)
{
    std::unique_lock<std::mutex> lock(m_packet_l.m_mtx);
    m_state.m_ps = status;
    lock.unlock();
    m_packet_l.m_cond.notify_all();
}
void whfa::Decoder::set_frame_status(FrameStatus status)
{
    std::unique_lock<std::mutex> lock(m_frame_l.m_mtx);
    m_state.m_fs = status;
    lock.unlock();
    m_frame_l.m_cond.notify_all();
}

void whfa::Decoder::set_decoder_averr(int averr)
{
    std::unique_lock<std::mutex> lock(m_decoder_l.m_mtx);
    m_state.m_ds = DecoderStatus::AVERROR;
    m_state.m_d_averr = averr;
    lock.unlock();
    m_decoder_l.m_cond.notify_all();
}

void whfa::Decoder::set_packet_averr(int averr)
{
    std::unique_lock<std::mutex> lock(m_packet_l.m_mtx);
    m_state.m_ps = PacketStatus::AVERROR;
    m_state.m_p_averr = averr;
    lock.unlock();
    m_packet_l.m_cond.notify_all();
}

void whfa::Decoder::set_frame_averr(int averr)
{
    std::unique_lock<std::mutex> lock(m_frame_l.m_mtx);
    m_state.m_fs = FrameStatus::AVERROR;
    m_state.m_f_averr = averr;
    lock.unlock();
    m_frame_l.m_cond.notify_all();
}
