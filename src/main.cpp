/*
test main method to read an encoded file and write a decoded file
many TODOs
eventually will be replaced by ALSA functionality, may come back later as a file-writing option

TODO: building on WSL still has linking errors, makefile was hacked together to just find compile-time errors better

@author Robert Griffith
*/

#include "whfa/decoder.h"
#include "whfa/reader.h"

#include <iostream>
#include <fstream>

constexpr size_t CAP_QUEUE_P = 500;
constexpr size_t CAP_QUEUE_F = 500;

constexpr size_t ERRBUFSZ = 256;

namespace
{
    char errbuf[ERRBUFSZ];

    void print_averror(int averror)
    {
        if (av_strerror(averror, errbuf, ERRBUFSZ) != 0)
        {
            snprintf(errbuf, ERRBUFSZ, "no error description found");
        }
        std::cerr << "AVERROR (" << averror << ") : " << errbuf << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    whfa::Context c(CAP_QUEUE_F, CAP_QUEUE_P);
    whfa::Reader r(c);
    whfa::Decoder d(c);

    int rv;

    std::cout << "initializing libav formats & networking" << std::endl;
    whfa::Context::register_formats();
    whfa::Context::enable_networking();

    std::cout << "openining " << argv[1] << std::endl;

    rv = c.open(argv[1]);
    if (rv != 0)
    {
        std::cerr << "failed to open" << std::endl;
        print_averror(rv);
        return 1;
    }

    AVSampleFormat format;
    int channels, rate;
    if (!c.get_sample_spec(format, channels, rate))
    {
        std::cerr << "failed to get sample specification after seemingly successful open" << std::endl;
    }
    bool is_planar = av_sample_fmt_is_planar(format) == 1;
    util::DualBlockingQueue<AVFrame *> *queue = c.get_frame_queue();

    r.start();
    d.start();

    std::ofstream ofs;
    ofs.open(argv[2], std::ios::binary | std::ios::out);

    // TODO: get frames on timeout (need to implement simple method i DualBlockigQueue)
    // TODO: determine EOF by Reader and Decoder states not running and with averror still set to 0
    // TODO: put frame popping and PCM format handling into new whfa class (probably a separate Threader)
    // TODO: use main thread to process seeking, starting, pausing, stopping, and other user input
    AVFrame *frame;
    whfa::Threader::State state;
    do
    {
        d.get_state(state);
        if (state.averror != 0)
        {
            std::cerr << "error in Decoder state" << std::endl;
            print_averror(state.averror);
            ofs.close();
            return 1;
        }
        else if (!state.run)
        {
            std::cout << "EOF reached" << std::endl;
            break;
        }
        if (!queue->pop(frame))
        {
            std::cerr << "pop failed, context flushed?" << std::endl;
            break;
        }
        // TODO: switch on format, pack frame properly into PCM data using the spec
        // TODO: configurably write to file or write to sound card
        // (for now just writig frame buffers to file)
        size_t framesz = frame->linesize[0] * channels;
        bool alloc;
        uint8_t *buf;
        if (is_planar && channels > 1)
        {
            // TODO: interleave according to full sample bitwidth, not just byte by byte
            // TODO: determine if ALSA and the DAC can handle planar formats, and I don't need to do this (maybe make it a config option)
            alloc = true;
            buf = new uint8_t[framesz];
            for (int c = 0; c < channels; ++c)
            {
                for (int i = 0; i < frame->linesize[0]; ++i)
                {
                    buf[i * channels + c] = frame->extended_data[c][i];
                }
            }
        }
        else
        {
            // already interleaved, leave it as is
            alloc = false;
            buf = frame->extended_data[0];
        }
        ofs.write((const char *)buf, framesz);
        if (alloc)
        {
            delete buf;
        }
        av_frame_free(&frame);
    } while (true);

    ofs.close();
    return 0;
}