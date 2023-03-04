/**
 * @file writer.cpp
 * @author Robert Griffith
 */
#include "pcm/writer.h"

#include <array>

namespace
{

    /// @brief suffix used for raw metadata files
    constexpr const char *__METADATA_SFX = ".meta";
    /// @brief wave chunk format tag for pcm data
    constexpr uint16_t __WAVFMT_PCM = 0x0001;
    /// @brief wave chunk format tag for float data
    constexpr uint16_t __WAVFMT_FLT = 0x0003;
    /// @brief number of supported libav formats
    constexpr size_t __NUM_AVFMTS = 10;

    /// @brief convenience alias for stream spec
    using WPCStreamSpec = whfa::pcm::Context::StreamSpec;
    /// @brief convenience alias for frame handler
    using WPFrameHandler = whfa::pcm::FrameHandler;
    /// @brief array type for wav format mapping
    using WavFormatMap = std::array<uint16_t, __NUM_AVFMTS>;

    /**
     * @brief compile time construction of wav format type map
     *
     * libav format -> wav chunk format tag
     */
    constexpr WavFormatMap constWavFormatMap()
    {
        WavFormatMap map = {0};
        // 8 bit
        map[AV_SAMPLE_FMT_U8] = __WAVFMT_PCM;
        map[AV_SAMPLE_FMT_U8P] = __WAVFMT_PCM;
        // 16 bit
        map[AV_SAMPLE_FMT_S16] = __WAVFMT_PCM;
        map[AV_SAMPLE_FMT_S16P] = __WAVFMT_PCM;
        // 24 or 32 bit
        map[AV_SAMPLE_FMT_S32] = __WAVFMT_PCM;
        map[AV_SAMPLE_FMT_S32P] = __WAVFMT_PCM;
        // float
        map[AV_SAMPLE_FMT_FLT] = __WAVFMT_FLT;
        map[AV_SAMPLE_FMT_FLTP] = __WAVFMT_FLT;
        // double
        map[AV_SAMPLE_FMT_DBL] = __WAVFMT_FLT;
        map[AV_SAMPLE_FMT_DBLP] = __WAVFMT_FLT;
        // 64 bit not supported
        return map;
    }

    /// @brief libav format -> wav chunk format tag
    constexpr const WavFormatMap __WAVFMT_MAP = constWavFormatMap();

    /**
     * @brief get number of bytes required to contain bitdepth bits (rounding up)
     *
     * @param bitdepth number of bits
     * @return number of bytes
     */
    inline int get_bytedepth(int bitdepth)
    {
        const int b = bitdepth >> 3;
        const int r = bitdepth & 0x7;
        return b + std::min<int>(r, 1);
    }

    /**
     * @class FileWriter
     * @brief small class to handle efficient varitations of writing to file
     */
    class FileWriter : public WPFrameHandler
    {
    public:
        /**
         * @brief constructor
         *
         * @param ofs open output file stream to write to
         * @param spec context stream specification
         */
        FileWriter(std::ofstream &ofs, const WPCStreamSpec &spec)
            : _ofs(&ofs),
              _bw(av_get_bytes_per_sample(spec.format))
        {
        }

    protected:
        /// @brief output file stream
        std::ofstream *_ofs;
        /// @brief bytewidth of individual channel sample
        const int _bw;
    };

    /**
     * @class WPWFileWriterSS
     * @brief base class for file writers handling subsample data
     */
    class FileWriterSS : public FileWriter
    {
    public:
        FileWriterSS(std::ofstream &ofs, const WPCStreamSpec &spec)
            : FileWriter(ofs, spec),
              _bd(get_bytedepth(spec.bitdepth))
        {
        }

    protected:
        /// @brief bytedepth of individual channel sample
        const int _bd;
    };

    /**
     * @class FWFullSampleI
     * @brief class for writing full frame of interleaved samples
     */
    class FWFullSampleI : public FileWriter
    {
    public:
        using FileWriter::FileWriter;

        int handle(const AVFrame &frame) override
        {
            const std::streamsize framesz = _bw * frame.nb_samples * frame.channels;
            const char *data = reinterpret_cast<const char *>(frame.extended_data[0]);
            _ofs->write(data, framesz);
            return *_ofs ? 0 : _ofs->rdstate();
        }
    };

    /**
     * @class FWFullSampleP
     * @brief class for writing full frame of planar samples
     */
    class FWFullSampleP : public FileWriter
    {
    public:
        using FileWriter::FileWriter;

        int handle(const AVFrame &frame) override
        {
            const int planesz = _bw * frame.nb_samples;
            for (int i = 0; i < planesz; i += _bw)
            {
                for (int c = 0; c < frame.channels; ++c)
                {
                    const char *data = reinterpret_cast<const char *>(&(frame.extended_data[c][i]));
                    _ofs->write(data, _bw);
                }
            }
            return *_ofs ? 0 : _ofs->rdstate();
        }
    };

    /**
     * @class FWSubSampleI
     * @brief class for writing frame of interleaved samples extracted from larger width
     */
    class FWSubSampleI : public FileWriterSS
    {
    public:
        using FileWriterSS::FileWriterSS;

        int handle(const AVFrame &frame) override
        {
            const int offset = _bw - _bd;
            const int framesz = _bw * frame.nb_samples * frame.channels;
            for (int i = offset; i < framesz; i += _bw)
            {
                const char *data = reinterpret_cast<const char *>(&(frame.extended_data[0][i]));
                _ofs->write(data, _bd);
            }
            return *_ofs ? 0 : _ofs->rdstate();
        }
    };

    /**
     * @class FWSubSampleP
     * @brief class for wirting frame of planar samples extracted from larger width
     */
    class FWSubSampleP : public FileWriterSS
    {
    public:
        using FileWriterSS::FileWriterSS;

        int handle(const AVFrame &frame) override
        {
            const int offset = _bw - _bd;
            const int planesz = _bw * frame.nb_samples;
            for (int i = offset; i < planesz; i += _bw)
            {
                for (int c = 0; c < frame.channels; ++c)
                {
                    const char *data = reinterpret_cast<const char *>(&(frame.extended_data[c][i]));
                    _ofs->write(data, _bd);
                }
            }
            return *_ofs ? 0 : _ofs->rdstate();
        }
    };

    /**
     * @brief write 2 byte value to output file stream
     *
     * @param ofs open output file stream
     * @param u 2 byte value to write
     */
    inline void write_2(std::ofstream &ofs, const uint16_t &u)
    {
        ofs.write(reinterpret_cast<const char *>(&u), 2);
    }

    /**
     * @brief write low 2 bytes of int value to output file stream
     *
     * @param ofs open output file stream
     * @param i int containing 2 byte value to write
     */
    inline void write_2(std::ofstream &ofs, const int &i)
    {
        ofs.write(reinterpret_cast<const char *>(&i), 2);
    }

    /**
     * @brief write 4 byte value to output file stream
     *
     * @param ofs open output file stream
     * @param u 4 byte value to write
     */
    inline void write_4(std::ofstream &ofs, const uint32_t &u)
    {
        ofs.write(reinterpret_cast<const char *>(&u), 4);
    }

    /**
     * @brief write int 4 byte value to output file stream
     *
     * @param ofs open output file stream
     * @param u int containing 4 byte value to write
     */
    inline void write_4(std::ofstream &ofs, const int &i)
    {
        ofs.write(reinterpret_cast<const char *>(&i), 4);
    }

    /**
     * @brief open file to write raw data to, and open and populate metadata file
     *
     * @param ofs output file stream to open
     * @param filepath file location to open
     * @param spec context stream specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_raw(std::ofstream &ofs, const char *filepath, const WPCStreamSpec &spec)
    {
        std::string filepath_md(filepath);
        filepath_md.append(__METADATA_SFX);
        std::ofstream ofs_md(filepath_md);
        if (!ofs_md)
        {
            return ofs_md.rdstate();
        }

        ofs_md << ".format = " << spec.format << std::endl;
        ofs_md << ".timebase.num = " << spec.timebase.num << std::endl;
        ofs_md << ".timebase.den = " << spec.timebase.den << std::endl;
        ofs_md << ".duration = " << spec.duration << std::endl;
        ofs_md << ".bitdepth = " << spec.bitdepth << std::endl;
        ofs_md << ".channels = " << spec.channels << std::endl;
        ofs_md << ".rate = " << spec.rate << std::endl;

        ofs_md.close();
        ofs.open(filepath, std::ios::out | std::ios::binary);
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief open WAV file and write header (total file size filled in later)
     *
     * @param ofs output file stream to open and write to
     * @param filepath file location to open
     * @param spec context stream specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_wav(std::ofstream &ofs, const char *filepath, const WPCStreamSpec &spec)
    {
        ofs.open(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs)
        {
            return ofs.rdstate();
        }

        const int blockcnt = static_cast<int>(av_rescale_q(spec.duration, spec.timebase, {1, spec.rate}));
        const int blocksz = spec.channels * get_bytedepth(spec.bitdepth);
        const int datasz = blockcnt * blocksz;

        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __WAVFMT_MAP.size())
        {
            // unsupported format
            return -1;
        }

        const uint16_t fmt = __WAVFMT_MAP[fmt_idx];
        int chunksz_fmt;
        switch (fmt)
        {
        case __WAVFMT_PCM:
            chunksz_fmt = 16;
            break;
        case __WAVFMT_FLT:
        default:
            chunksz_fmt = 18;
            break;
        }
        int chunksz_riff = 4 + 8 + chunksz_fmt + 8 + datasz;
        switch (fmt)
        {
        case __WAVFMT_PCM:
            // no extra chunks to add to riff chunk size
            break;
        case __WAVFMT_FLT:
        default:
            chunksz_riff += 8 + 4;
            break;
        }
        const int padsz = chunksz_riff & 1;

        ofs.write("RIFF", 4);
        write_4(ofs, static_cast<uint32_t>(chunksz_riff + padsz));
        ofs.write("WAVE", 4);

        ofs.write("fmt ", 4);
        write_4(ofs, chunksz_fmt);
        write_2(ofs, fmt);
        write_2(ofs, spec.channels);
        write_4(ofs, spec.rate);
        write_4(ofs, spec.rate * blocksz);
        write_2(ofs, blocksz);
        write_2(ofs, spec.bitdepth);
        switch (fmt)
        {
        case __WAVFMT_PCM:
            // no extra chunks to write before data
            break;
        case __WAVFMT_FLT:
        default:
            write_2(ofs, 0);
            ofs.write("fact", 4);
            write_4(ofs, 4);
            write_4(ofs, spec.channels * blockcnt);
        }

        ofs.write("data", 4);
        write_4(ofs, datasz);

        // data and pad bit
        const int bufsz = datasz + padsz;
        const char *buf = new char[bufsz]();
        ofs.write(buf, bufsz);
        ofs.seekp(-bufsz, std::ios_base::cur);
        delete buf;
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief select file writer according to stream specification
     *
     * @param ofs output file stream
     * @param spec stream specification
     * @return file writer, nullptr on invalid/unsupported spec
     */
    WPFrameHandler *get_file_writer(std::ofstream &ofs, const WPCStreamSpec &spec)
    {
        const int bw = av_get_bytes_per_sample(spec.format) << 3;
        const bool subsample = spec.bitdepth < bw;
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        WPFrameHandler *fw = nullptr;
        if (subsample)
        {
            if (planar)
            {
                fw = static_cast<WPFrameHandler *>(new FWSubSampleP(ofs, spec));
            }
            else
            {
                fw = static_cast<WPFrameHandler *>(new FWSubSampleI(ofs, spec));
            }
        }
        else
        {
            if (planar)
            {

                fw = static_cast<WPFrameHandler *>(new FWFullSampleP(ofs, spec));
            }
            else
            {

                fw = static_cast<WPFrameHandler *>(new FWFullSampleI(ofs, spec));
            }
        }
        return fw;
    }

}

namespace whfa::pcm
{

    /**
     * whfa::pcm::Writer public methods
     */

    Writer::Writer(Context &context)
        : Worker(context),
          _mode(OutputType::FILE_RAW),
          _writer(nullptr)
    {
    }

    Writer::~Writer()
    {
        close();
    }

    bool Writer::open(const char *filepath, OutputType mode)
    {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_ofs.is_open())
        {
            _ofs.close();
        }

        _mode = mode;

        if (!_ctxt->get_stream_spec(_spec))
        {
            set_state_stop(Worker::FORMATINVAL | Worker::CODECINVAL);
            return false;
        }

        int rv = 0;
        switch (_mode)
        {
        case FILE_RAW:
            rv = open_file_raw(_ofs, filepath, _spec);
            break;
        case FILE_WAV:
            rv = open_file_wav(_ofs, filepath, _spec);
            break;
        }
        _writer = get_file_writer(_ofs, _spec);
        const bool err = rv != 0;
        if (err)
        {
            set_state_stop(rv);
            if (_ofs.is_open())
            {
                _ofs.close();
            }
        }
        return !err;
    }

    void Writer::close()
    {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_ofs.is_open())
        {
            _ofs.close();
        }
        set_state_stop();
    }

    /**
     * whfa::pcm::Writer protected methods
     */

    void Writer::execute_loop_body()
    {
        if (!_ofs.is_open())
        {
            set_state_stop();
            return;
        }
        else if (!_ofs)
        {
            set_state_pause(_ofs.rdstate());
            return;
        }

        AVFrame *frame;
        if (!_ctxt->get_frame_queue().pop(frame))
        {
            // due to flush, not an error state
            return;
        }
        if (frame == nullptr)
        {
            // EOF, stop and close (no queue to forward to)
            set_state_stop();
            _ofs.close();
            return;
        }

        const int rv = _writer->handle(*frame);
        set_state_timestamp(frame->pts);
        av_frame_free(&frame);
        if (rv != 0)
        {
            set_state_pause(rv);
        }
    }

}