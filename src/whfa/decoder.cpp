/**
 * @file decoder.cpp
 * @author Robert Griffith
 */

#include "decoder.h"

/**
 * whfa::Decoder public methods
 */

whfa::Decoder::Decoder(Context &context)
    : Worker(context)
{
}

/**
 * whfa::Decoder protected methods
 */

void whfa::Decoder::execute_loop_body()
{
    std::mutex *cdc_mtx;
    AVCodecContext *cdc_ctxt;
    cdc_mtx = _ctxt->get_codec(cdc_ctxt);
    if (cdc_mtx == nullptr)
    {
        set_state_stop();
        set_state_error(Worker::CODECINVAL);
        return;
    }

    util::DBPQueue<AVPacket> &pkt_queue = _ctxt->get_packet_queue();
    util::DBPQueue<AVFrame> &fr_queue = _ctxt->get_frame_queue();
    AVPacket *packet;
    if (!pkt_queue.pop(packet))
    {
        // due to flush, not an error state
        cdc_mtx->unlock();
        return;
    }

    int rv = avcodec_send_packet(cdc_ctxt, packet);
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
                    _state.timestamp = frame->pts;
                    frame = nullptr;
                }
            }
            if (frame != nullptr)
            {
                // flush, error, or no more frames in packet
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
    cdc_mtx->unlock();
}