/**
 * @file main.cpp
 * @author Robert Griffith
 *
 * @todo: move into whfa::Writer
 * @todo: implement cli and test methods
 * @todo: building on WSL still has linking errors, makefile was hacked together to just find compile-time errors better
 */
#include "whfa/decoder.h"
#include "whfa/reader.h"
#include "whfa/writer.h"

#include <iostream>
#include <fstream>

namespace
{
    constexpr size_t __CAP_QUEUE_P = 500;
    constexpr size_t __CAP_QUEUE_F = 500;

    constexpr size_t __ERRBUFSZ = 256;

    constexpr const char *__OUTDEV = "default";
    constexpr const char *__OUTWAV = "out.wav";
    constexpr const char *__OUTRAW = "out.raw";

    char __errbuf[__ERRBUFSZ];

    void print_error(int averror)
    {
        if (av_strerror(averror, __errbuf, __ERRBUFSZ) != 0)
        {
            snprintf(__errbuf, __ERRBUFSZ, "no error description found");
        }
        std::cerr << "AVERROR (" << averror << ") : " << __errbuf << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        return 1;
    }

    whfa::Context c(__CAP_QUEUE_F, __CAP_QUEUE_P);
    whfa::Reader r(c);
    whfa::Decoder d(c);
    whfa::Writer w(c);
    util::Threader::State state;

    int rv;

    std::cout << "initializing libav formats & networking" << std::endl;
    whfa::Context::register_formats();
    whfa::Context::enable_networking();

    std::cout << "openining " << argv[1] << std::endl;

    rv = c.open(argv[1]);
    if (rv != 0)
    {
        std::cerr << "failed to open input" << std::endl;
        print_error(rv);
        return 1;
    }

    if (!w.open(__OUTWAV, whfa::Writer::OutputType::FILE_RAW))
    {
        std::cerr << "failed to open output" << std::endl;
        w.get_state(state);
        print_error(state.error);
        return 1;
    }

    // TODO: use writer logic w.write_to_file(filename, spec);

    whfa::Context::StreamSpec spec;
    if (!c.get_stream_spec(spec))
    {
        std::cerr << "failed to get stream spec after open" << std::endl;
        return 1;
    }

    bool is_planar = av_sample_fmt_is_planar(spec.format) == 1;
    util::DBPQueue<AVFrame> &queue = c.get_frame_queue();
    const std::chrono::microseconds timeout(10); // TODO: configure frame pop timeout to sample frequency

    w.start();
    d.start();
    r.start();

    std::ofstream ofs;
    ofs.open(argv[2], std::ios::binary | std::ios::out);

    // TODO: determine EOF by Reader and Decoder states not running and with averror still set to 0
    // TODO: put frame popping and PCM format handling into new whfa class (probably a separate Threader)
    // TODO: use main thread to process seeking, starting, pausing, stopping, and other user input
    AVFrame *frame;
    do
    {
        d.get_state(state);
        if (state.error != 0)
        {
            std::cerr << "error in Decoder state" << std::endl;
            print_error(state.error);
            ofs.close();
            return 1;
        }
        else if (!state.run)
        {
            std::cout << "EOF reached" << std::endl;
            break;
        }
        if (!queue.pop(frame, timeout))
        {
            std::cerr << "pop failed, context flushed?" << std::endl;
            break;
        }
        // TODO: switch on format, pack frame properly into PCM data using the spec
        // TODO: configurably write to file or write to sound card
        // (for now just writig frame buffers to file)
        size_t framesz = frame->linesize[0] * spec.channels;
        bool alloc;
        uint8_t *buf;
        if (is_planar && spec.channels > 1)
        {
            // TODO: interleave according to full sample bitwidth, not just byte by byte
            // TODO: determine if ALSA and the DAC can handle planar formats, and I don't need to do this (maybe make it a config option)
            alloc = true;
            buf = new uint8_t[framesz];
            for (int c = 0; c < spec.channels; ++c)
            {
                for (int i = 0; i < frame->linesize[0]; ++i)
                {
                    buf[i * spec.channels + c] = frame->extended_data[c][i];
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