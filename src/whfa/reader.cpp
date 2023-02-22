/**
 * @file reader.cpp
 * @author Robert Griffith
 */
#include "reader.h"

namespace
{

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

}

/**
 * whfa::Reader public methods
 */

whfa::Reader::Reader(Context &context)
    : Worker(context)
{
}

bool whfa::Reader::seek(int64_t pos_pts)
{
    std::lock_guard<std::mutex> lk(_mtx);
    int err = 0;
    bool stop = false;

    AVFormatContext *fmt_ctxt;
    int s_idx;
    std::mutex *fmt_mtx = _ctxt->get_format(fmt_ctxt, s_idx);
    if (fmt_mtx == nullptr)
    {
        err = Worker::FORMATINVAL;
        stop = true;
    }
    else
    {
        const int64_t conv_pts = av_rescale_q(pos_pts, AV_TIME_BASE_Q,
                                              fmt_ctxt->streams[s_idx]->time_base);
        const int flags = (conv_pts < _state.timestamp) ? AVSEEK_FLAG_BACKWARD : 0;
        const int rv = av_seek_frame(fmt_ctxt, s_idx, conv_pts, flags);
        _ctxt->get_packet_queue().flush(free_packet);
        fmt_mtx->unlock();

        if (rv < 0)
        {
            err = rv;
        }
        else
        {
            AVCodecContext *cdc_ctxt;
            std::mutex *cdc_mtx = _ctxt->get_codec(cdc_ctxt);
            if (cdc_mtx == nullptr)
            {
                err = Worker::CODECINVAL;
                stop = true;
            }
            else
            {
                avcodec_flush_buffers(cdc_ctxt);
                _ctxt->get_frame_queue().flush(free_frame);
                cdc_mtx->unlock();
            }
        }
    }
    if (err != 0)
    {
        if (stop)
        {
            set_state_stop();
        }
        else
        {
            set_state_pause();
        }
        set_state_error(err);
        return false;
    }
    return true;
}

bool whfa::Reader::seek(double pos_pct)
{
    std::lock_guard<std::mutex> lk(_mtx);
    int err = 0;
    bool stop = false;

    AVFormatContext *fmt_ctxt;
    int s_idx;
    std::mutex *fmt_mtx = _ctxt->get_format(fmt_ctxt, s_idx);
    if (fmt_mtx == nullptr)
    {
        err = Worker::FORMATINVAL;
        stop = true;
    }
    else
    {
        const int64_t conv_pts = static_cast<int64_t>(pos_pct * fmt_ctxt->streams[s_idx]->duration);
        const int flags = (conv_pts < _state.timestamp) ? AVSEEK_FLAG_BACKWARD : 0;
        const int rv = av_seek_frame(fmt_ctxt, s_idx, conv_pts, flags);
        _ctxt->get_packet_queue().flush();
        fmt_mtx->unlock();

        if (rv < 0)
        {
            err = rv;
        }
        else
        {
            AVCodecContext *cdc_ctxt;
            std::mutex *cdc_mtx = _ctxt->get_codec(cdc_ctxt);
            if (cdc_mtx == nullptr)
            {
                err = Worker::CODECINVAL;
                stop = true;
            }
            else
            {
                avcodec_flush_buffers(cdc_ctxt);
                _ctxt->get_frame_queue().flush();
                cdc_mtx->unlock();
            }
        }
    }
    if (err != 0)
    {
        if (stop)
        {
            set_state_stop();
        }
        else
        {
            set_state_pause();
        }
        set_state_error(err);
        return false;
    }
    return true;
}

/**
 * whfa::Reader protected methods
 */

void whfa::Reader::execute_loop_body()
{
    std::mutex *fmt_mtx;
    AVFormatContext *fmt_ctxt;
    int s_idx;
    fmt_mtx = _ctxt->get_format(fmt_ctxt, s_idx);
    if (fmt_mtx == nullptr)
    {
        set_state_stop();
        set_state_error(Worker::FORMATINVAL);
        return;
    }

    util::DBPQueue<AVPacket> &queue = _ctxt->get_packet_queue();
    AVPacket *packet = av_packet_alloc();
    int rv;
    while ((rv = av_read_frame(fmt_ctxt, packet)) == 0)
    {
        if (packet->stream_index == s_idx)
        {
            if (queue.push(packet))
            {
                _state.timestamp = packet->pts + packet->duration;
                packet = nullptr;
            }
            break;
        }
    }
    fmt_mtx->unlock();
    if (packet != nullptr)
    {
        // flush or no desired packet found, not an error state
        av_packet_free(&packet);
    }
    if (rv == AVERROR_EOF)
    {
        set_state_stop();
    }
    else if (rv != 0)
    {
        set_state_pause();
        set_state_error(rv);
    }
}
