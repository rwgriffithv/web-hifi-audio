/**
 * @file reader.cpp
 * @author Robert Griffith
 */
#include "pcm/reader.h"

namespace
{
    
    /// @brief convenience alias for std::min and std::max
    using Choose64 = const int64_t &(*)(const int64_t &, const int64_t &);
    /// @brief convenience alias for std::min<int64_t>
    constexpr Choose64 min_i64 = std::min<int64_t>;
    /// @brief convenience alias for std::max<int64_t>
    constexpr Choose64 max_i64 = std::max<int64_t>;

}

namespace whfa::pcm
{

    /**
     * whfa::pcm::Reader public methods
     */

    Reader::Reader(Context &context)
        : Worker(context)
    {
    }

    bool Reader::seek(int64_t pos_pts)
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
            const int64_t dur_pts = fmt_ctxt->streams[s_idx]->duration;
            const int64_t conv_pts = av_rescale_q(pos_pts, AV_TIME_BASE_Q,
                                                  fmt_ctxt->streams[s_idx]->time_base);
            const int64_t clip_pts = min_i64(max_i64(conv_pts, 0), dur_pts);
            const int flags = (clip_pts < get_state().timestamp) ? AVSEEK_FLAG_BACKWARD : 0;
            const int rv = av_seek_frame(fmt_ctxt, s_idx, clip_pts, flags);
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
                set_state_stop(err);
            }
            else
            {
                set_state_pause(err);
            }
            return false;
        }
        return true;
    }

    bool Reader::seek(double pos_pct)
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
            const int64_t dur_pts = fmt_ctxt->streams[s_idx]->duration;
            const int64_t conv_pts = static_cast<int64_t>(pos_pct * dur_pts);
            const int64_t clip_pts = min_i64(max_i64(conv_pts, 0), dur_pts);
            const int flags = (clip_pts < get_state().timestamp) ? AVSEEK_FLAG_BACKWARD : 0;
            const int rv = av_seek_frame(fmt_ctxt, s_idx, clip_pts, flags);
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
                set_state_stop(err);
            }
            else
            {
                set_state_pause(err);
            }
            return false;
        }
        return true;
    }

    /**
     * whfa::pcm::Reader protected methods
     */

    void Reader::execute_loop_body()
    {
        util::DBPQueue<AVPacket> &pkt_queue = _ctxt->get_packet_queue();

        std::mutex *fmt_mtx;
        AVFormatContext *fmt_ctxt;
        int s_idx;
        fmt_mtx = _ctxt->get_format(fmt_ctxt, s_idx);
        if (fmt_mtx == nullptr)
        {
            set_state_stop(Worker::FORMATINVAL);
            return;
        }

        AVPacket *packet = av_packet_alloc();
        int rv;
        while ((rv = av_read_frame(fmt_ctxt, packet)) == 0)
        {
            if (packet->stream_index == s_idx)
            {
                if (pkt_queue.push(packet))
                {
                    set_state_timestamp(packet->pts + packet->duration);
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
            // EOF, stop and forward
            set_state_stop();
            while (!pkt_queue.push(nullptr))
            {
            }
        }
        else if (rv != 0)
        {
            set_state_pause(rv);
        }
    }

}