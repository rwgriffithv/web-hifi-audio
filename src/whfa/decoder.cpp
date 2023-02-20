/*
@author Robert Griffith
*/

#include "decoder.h"

/*
public methods
*/

whfa::Decoder::Decoder(Context &context)
    : Worker(context)
{
}

/*
protected methods
*/

void whfa::Decoder::thread_loop_body()
{
    std::mutex *cdc_lock;
    AVCodecContext *cdc_ctxt;
    cdc_lock = _ctxt->get_codec(&cdc_ctxt);
    if (cdc_lock == nullptr)
    {
        set_state_stop();
        return;
    }

    util::DBPQueue<AVPacket> &pkt_queue = _ctxt->get_packet_queue();
    util::DBPQueue<AVFrame> &fr_queue = _ctxt->get_frame_queue();
    AVPacket *packet;
    if (!pkt_queue.pop(packet))
    {
        return;
    }

    int rv;
    rv = avcodec_send_packet(cdc_ctxt, packet);
    if (rv == 0 || rv == AVERROR(EAGAIN))
    {
        do
        {
            AVFrame *frame = av_frame_alloc();
            rv = avcodec_receive_frame(cdc_ctxt, frame);
            if (rv == 0)
            {
                if (fr_queue.push(frame))
                {
                    _state.curr_pts = frame->pts;
                    frame = nullptr;
                }
            }
            if (frame != nullptr)
            {
                av_frame_free(&frame);
            }
        } while (rv == 0);
        if (rv != AVERROR(EAGAIN))
        {
            set_state_pause();
            set_state_error(rv);
        }
    }
    else
    {
        set_state_pause();
        set_state_error(rv);
    }
    cdc_lock->unlock();
}