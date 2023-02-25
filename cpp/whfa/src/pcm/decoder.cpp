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
    util::DBPQueue<AVPacket> &pkt_queue = _ctxt->get_packet_queue();
    util::DBPQueue<AVFrame> &frm_queue = _ctxt->get_frame_queue();
    AVPacket *packet;
    if (!pkt_queue.pop(packet))
    {
        // due to flush, not an error state
        return;
    }
    if (packet == nullptr)
    {
        // EOF, stop & forward
        set_state_stop();
        while (!frm_queue.push(nullptr))
        {
        }
        return;
    }

    AVCodecContext *cdc_ctxt;
    std::mutex *cdc_mtx = _ctxt->get_codec(cdc_ctxt);
    if (cdc_mtx == nullptr)
    {
        set_state_stop(Worker::CODECINVAL);
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
                if (frm_queue.push(frame))
                {
                    set_state_timestamp(frame->pts);
                    frame = nullptr;
                }
            }
            if (frame != nullptr)
            {
                // flush, error, or no more frames in packet
                av_frame_free(&frame);
            }
        } while (rv == 0);
    }
    cdc_mtx->unlock();
    
    if (rv != AVERROR(EAGAIN))
    {
        set_state_pause(rv);
    }
}