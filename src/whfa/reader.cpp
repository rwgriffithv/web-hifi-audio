/*
@author Robert Griffith
*/

#include "reader.h"

/*
public methods
*/

whfa::Reader::Reader(Context &context)
    : Worker(context)
{
}

bool whfa::Reader::seek(int64_t pos_pts)
{
    std::lock_guard<std::mutex> lock(_mtx);
    std::mutex *fmt_lock;
    AVFormatContext *fmt_ctxt;
    int stream_idx;
    fmt_lock = _ctxt->get_format(&fmt_ctxt, &stream_idx);
    if (fmt_lock == nullptr)
    {
        return false;
    }

    int64_t conv_pts = av_rescale_q(pos_pts, AV_TIME_BASE_Q,
                                    fmt_ctxt->streams[stream_idx]->time_base);
    int flags = (conv_pts < _state.curr_pts) ? AVSEEK_FLAG_BACKWARD : 0;
    int rv = av_seek_frame(fmt_ctxt, stream_idx, conv_pts, flags);
    if (rv >= 0)
    {
        // flush contexts and queues
        _ctxt->flush();
    }
    fmt_lock->unlock();
    if (rv < 0)
    {
        set_state_error(rv);
        set_state_pause();
        return false;
    }
    return true;
}

/*
protected methods
*/

void whfa::Reader::thread_loop_body()
{
    std::mutex *fmt_lock;
    AVFormatContext *fmt_ctxt;
    int stream_idx;
    fmt_lock = _ctxt->get_format(&fmt_ctxt, &stream_idx);
    if (fmt_lock == nullptr)
    {
        set_state_stop();
        return;
    }

    util::DBPQueue<AVPacket> &queue = _ctxt->get_packet_queue();
    AVPacket *packet = av_packet_alloc();
    int rv;
    while ((rv = av_read_frame(fmt_ctxt, packet)) == 0)
    {
        if (packet->stream_index == stream_idx)
        {
            if (queue.push(packet))
            {
                _state.curr_pts = packet->pts + packet->duration;
                packet = nullptr;
            }
            break;
        }
    }
    fmt_lock->unlock();
    if (packet != nullptr)
    {
        /*
        push failed due to queue flush (likely a seek)
        do not stop reading, cleanup packet
        */
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
