/*
web hi-fi audio class for decoding audio streams using libavformat and libavcodec

@author Robert Griffith

TODO: improve/simplify state, sychronization, & "interrupts", starting by implementing seeking
        (hopefully remove ugly and mostly unused private state methods)
TODO: finish thread signaling in destructor (after seeking impl)
*/

#include "util/dbqueue.h"

#include <thread>

#include <libavformat/avformat.h>

namespace whfa
{

    class Decoder
    {
    public:
        enum DecoderStatus
        {
            UNINIT,
            INIT,
            AVERROR,
            DESTRUCTING
        };
        enum PacketStatus
        {
            UNINIT,
            READING,
            END_OF_FILE,
            AVERROR,
            DESTRUCTING
        };
        enum FrameStatus
        {
            UNINIT,
            DECODING,
            AVERROR,
            DESTRUCTING
        };

        struct State
        {
            /*
            series of mutually exclusive status enums describing state
            */
            DecoderStatus m_ds; /* decoder class status */
            PacketStatus m_ps;  /* packet thread status */
            FrameStatus m_fs;   /* frame thread status */
            /*
            AVERROR values set when corresponding status is AVERROR on failure (to be used with libavutil/error.h:av_strerror)
            values will store last known AVERROR and are invalid unless status is AVERROR
            */
            int m_d_averr; /* decoder AVERROR */
            int m_p_averr; /* packet thread AVERROR */
            int m_f_averr; /* frame thread AVERROR */
        };

        static void register_formats();
        static void enable_networking();
        static void disable_networking();

        /*
        spawns packet and frame processing threads
        threads block and wait for initialization with URL
        @param packet_queue_capacity max capacity of packet queue in number of packets
        @param frame_queue_capacity max capacity of frame queue in number of frames
        */
        Decoder(size_t packet_queue_capacity, size_t frame_queue_capacity);

        /*
        invokes free() and joins packet and frame processing threads
        */
        ~Decoder();

        /*
        configures aduio format, codec, and frame(s) for decoding based upon file/stream being decoded
        @param url URL of the stream to open
        @param[out] format sample format from selected codec (valid only if returns true)
        @param[out] channels number of audio channels (valid only if returns true)
        @param[out] sample_rate the number of samples per second (valid only if returns true)
        @return true if successful, check state with get_state() if false
        */
        bool initialize(const char *url, AVSampleFormat &format, int &channels, int &sample_rate);

        /*
        stops threads and frees data stored for a particular file/stream being decoded
        initialize must be invoked again before decoding
        */
        void free();

        /*
        threadsafely access state and copy into struct
        @param[out] state decoder state to copy into
        */
        void get_state(State &state);

        /*
        get frame of decoded audio data ready for ALSA use
        blocks until frame queue has element to pop or there is an error
        */
        int get_frames(size_t count, AVFrame **frames);

    private:
        struct CondLock
        {

            std::mutex m_mtx;
            std::condition_variable m_cond;
        };

        void populate_packet_queue();
        void populate_frame_queue();

        void set_uninitialized_state();
        void set_initialized_state();

        void set_decoder_status(DecoderStatus status);
        void set_packet_status(PacketStatus status);
        void set_frame_status(FrameStatus status);

        void set_decoder_averr(int averr);
        void set_packet_averr(int averr);
        void set_frame_averr(int averr);

        State m_state;

        CondLock m_decoder_l;
        CondLock m_packet_l;
        CondLock m_frame_l;

        util::DualBlockingQueue<AVPacket *> m_packet_q;
        util::DualBlockingQueue<AVFrame *> m_frame_q;

        std::thread m_packet_t;
        std::thread m_frame_t;

        AVFormatContext *m_format_ctxt;
        int m_stream_idx;
        AVCodecContext *m_codec_ctxt;
    };
}
