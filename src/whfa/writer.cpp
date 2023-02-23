/**
 * @file writer.cpp
 * @author Robert Griffith
 *
 * @todo consider exposing device resampling and latency in public methods
 * @todo use map (array) of function pointers to handle writing different formats
 * @todo map indexed by spec.format
 */
#include "writer.h"

#include <array>

namespace
{
    /// @brief 1 = ALSA soft resampling, 0 = no resampling
    constexpr int __RESAMPLE = 0;
    /// @brief latency in device
    constexpr unsigned int __LATENCY_US = 500000;
    /// @brief suffix used for raw metadata files
    constexpr const char *__METADATA_SFX = ".meta";
    /// @brief wave chunk format tag for pcm data
    constexpr uint16_t __WAVFMT_PCM = 0x0001;
    /// @brief wave chunk format tag for float data
    constexpr uint16_t __WAVFMT_FLT = 0x0003;
    /// @brief number of supported libav formats
    constexpr size_t __NUM_AVFMTS = 10;

    using SndFormatMap = std::array<snd_pcm_format_t, __NUM_AVFMTS>;
    using WavFormatMap = std::array<uint16_t, __NUM_AVFMTS>;
    using DevWriterMap = std::array<whfa::Writer::DeviceWriter, __NUM_AVFMTS>;
    using FileWriterMap = std::array<whfa::Writer::FileWriter, __NUM_AVFMTS>;

    /**
     * @brief compile time construction of sample format type map
     *
     * libav format -> asoundlib format (24 bit needs help)
     */
    constexpr SndFormatMap constSampleFormatMap()
    {
        SndFormatMap map = {SND_PCM_FORMAT_UNKNOWN};
        // 8 bit
        map[AV_SAMPLE_FMT_U8] = SND_PCM_FORMAT_U8;
        map[AV_SAMPLE_FMT_U8P] = SND_PCM_FORMAT_U8;
        // 16 bit
        map[AV_SAMPLE_FMT_S16] = SND_PCM_FORMAT_S16;
        map[AV_SAMPLE_FMT_S16P] = SND_PCM_FORMAT_S16;
        // 24 or 32 bit
        map[AV_SAMPLE_FMT_S32] = SND_PCM_FORMAT_S32;
        map[AV_SAMPLE_FMT_S32P] = SND_PCM_FORMAT_S32;
        // float
        map[AV_SAMPLE_FMT_FLT] = SND_PCM_FORMAT_FLOAT;
        map[AV_SAMPLE_FMT_FLTP] = SND_PCM_FORMAT_FLOAT;
        // double
        map[AV_SAMPLE_FMT_DBL] = SND_PCM_FORMAT_FLOAT64;
        map[AV_SAMPLE_FMT_DBLP] = SND_PCM_FORMAT_FLOAT64;
        // 64 bit not supported
        return map;
    }

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

    /// @todo implement all device writers

    int write_dev_u8(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_u8_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s16(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s16_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s24(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s24_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s32(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_s32_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_flt(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_flt_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_dbl(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_dev_dbl_p(snd_pcm_t *handle, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }

    /**
     * @brief compile time construction of device writer map
     *
     * libav format -> device writer (24 bit needs help)
     */
    constexpr DevWriterMap constDevWriterMap()
    {
        DevWriterMap map = {nullptr};
        // 8 bit
        map[AV_SAMPLE_FMT_U8] = write_dev_u8;
        map[AV_SAMPLE_FMT_U8P] = write_dev_u8_p;
        // 16 bit
        map[AV_SAMPLE_FMT_S16] = write_dev_s16;
        map[AV_SAMPLE_FMT_S16P] = write_dev_s16_p;
        // 24 or 32 bit
        map[AV_SAMPLE_FMT_S32] = write_dev_s32;
        map[AV_SAMPLE_FMT_S32P] = write_dev_s32_p;
        // float
        map[AV_SAMPLE_FMT_FLT] = write_dev_flt;
        map[AV_SAMPLE_FMT_FLTP] = write_dev_flt_p;
        // double
        map[AV_SAMPLE_FMT_DBL] = write_dev_dbl;
        map[AV_SAMPLE_FMT_DBLP] = write_dev_dbl_p;
        // 64 bit not supported
        return map;
    }

    /// @todo implement all file writers

    int write_file_u8(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_u8_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s16(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s16_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s24(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s24_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s32(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_s32_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_flt(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_flt_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_dbl(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }
    int write_file_dbl_p(std::ofstream &ofs, const AVFrame &frame, const whfa::Context::StreamSpec &spec)
    {
        return -1;
    }

    /**
     * @brief compile time construction of file writer map
     *
     * libav format -> file writer (24 bit needs help)
     */
    constexpr FileWriterMap constFileWriterMap()
    {
        FileWriterMap map = {nullptr};
        // 8 bit
        map[AV_SAMPLE_FMT_U8] = write_file_u8;
        map[AV_SAMPLE_FMT_U8P] = write_file_u8_p;
        // 16 bit
        map[AV_SAMPLE_FMT_S16] = write_file_s16;
        map[AV_SAMPLE_FMT_S16P] = write_file_s16_p;
        // 24 or 32 bit
        map[AV_SAMPLE_FMT_S32] = write_file_s32;
        map[AV_SAMPLE_FMT_S32P] = write_file_s32_p;
        // float
        map[AV_SAMPLE_FMT_FLT] = write_file_flt;
        map[AV_SAMPLE_FMT_FLTP] = write_file_flt_p;
        // double
        map[AV_SAMPLE_FMT_DBL] = write_file_dbl;
        map[AV_SAMPLE_FMT_DBLP] = write_file_dbl_p;
        // 64 bit not supported
        return map;
    }

    /// @brief libav format -> asoundlib format
    constexpr const SndFormatMap __SNDFMT_MAP = constSampleFormatMap();
    /// @brief libav format -> wav chunk format tag
    constexpr const WavFormatMap __WAVFMT_MAP = constWavFormatMap();
    /// @brief libav format -> device writer function
    constexpr const DevWriterMap __DEVWFN_MAP = constDevWriterMap();
    /// @brief libav format -> file writer function
    constexpr const FileWriterMap __FILEWFN_MAP = constFileWriterMap();

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
     * @brief open device according to context stream specification
     *
     * @param[out] handle asoundlib pcm device handle
     * @param name device name
     * @param spec context stream specification
     * @return 0 if successful, error code otherwise
     */
    int open_dev(snd_pcm_t *&handle, const char *name, const whfa::Context::StreamSpec &spec)
    {
        int rv;
        if ((rv = snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        {
            return rv;
        }

        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __SNDFMT_MAP.size())
        {
            // unsupported format
            return -1;
        }

        snd_pcm_format_t format = __SNDFMT_MAP[fmt_idx];
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        const snd_pcm_access_t access = planar
                                            ? SND_PCM_ACCESS_RW_NONINTERLEAVED
                                            : SND_PCM_ACCESS_RW_INTERLEAVED;

        // special handling of 24 bit audio packed in 32 bits
        if (format == SND_PCM_FORMAT_S32 && spec.bitdepth == 24)
        {
            format = SND_PCM_FORMAT_S24;
        }

        if ((rv = snd_pcm_set_params(handle,
                                     format,
                                     access,
                                     spec.channels,
                                     spec.rate,
                                     __RESAMPLE,
                                     __LATENCY_US)) < 0)
        {
            return rv;
        }
        return 0;
    }

    /**
     * @brief open file to write raw data to, and open and populate metadata file
     *
     * @param ofs output file stream to open
     * @param name file location to open
     * @param spec context stream specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_raw(std::ofstream &ofs, const char *name, const whfa::Context::StreamSpec &spec)
    {
        std::string name_md(name);
        name_md.append(__METADATA_SFX);
        std::ofstream ofs_md(name_md);
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
        ofs.open(name, std::ios::out | std::ios::binary);
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief open WAV file and write header (total file size filled in later)
     *
     * @param ofs output file stream to open and write to
     * @param name file location to open
     * @param spec context stream specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_wav(std::ofstream &ofs, const char *name, const whfa::Context::StreamSpec &spec)
    {
        ofs.open(name, std::ios::out | std::ios::binary);
        if (!ofs)
        {
            return ofs.rdstate();
        }

        const int blockcnt = static_cast<int>(av_rescale_q(spec.duration, spec.timebase, {spec.rate, 1}));
        const int blocksz = spec.channels * spec.bitdepth >> 3;
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

        ofs.write("fmt\0", 4);
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
        const size_t bufsz = datasz + padsz;
        const char *buf = new char[bufsz]();
        ofs.write(buf, bufsz);
        ofs.seekp(-bufsz, std::ios_base::cur);
        delete buf;

        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief select device writer according to stream specification
     *
     * @param spec stream specification
     * @return device writer, nullptr on invalid/unsupported spec
     */
    whfa::Writer::DeviceWriter get_dev_writer(const whfa::Context::StreamSpec &spec)
    {
        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __DEVWFN_MAP.size())
        {
            // unsupported format
            return nullptr;
        }
        whfa::Writer::DeviceWriter writer = __DEVWFN_MAP[fmt_idx];
        // special handling of 24 bit audio packed in 32 bits
        if (spec.bitdepth == 24)
        {
            if (writer == write_dev_s32)
            {
                writer = write_dev_s24;
            }
            else if (writer == write_dev_s32_p)
            {
                writer = write_dev_s24_p;
            }
        }
        return writer;
    }

    /**
     * @brief select file writer according to stream specification
     *
     * @param spec stream specification
     * @return file writer, nullptr on invalid/unsupported spec
     */
    whfa::Writer::FileWriter get_file_writer(const whfa::Context::StreamSpec &spec)
    {
        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __FILEWFN_MAP.size())
        {
            // unsupported format
            return nullptr;
        }
        whfa::Writer::FileWriter writer = __FILEWFN_MAP[fmt_idx];
        // special handling of 24 bit audio packed in 32 bits
        if (spec.bitdepth == 24)
        {
            if (writer == write_file_s32)
            {
                writer = write_file_s24;
            }
            else if (writer == write_file_s32_p)
            {
                writer = write_file_s24_p;
            }
        }
        return writer;
    }
}

/**
 * whfa::Writer public methods
 */

whfa::Writer::Writer(whfa::Context &context)
    : Worker(context),
      _mode(OutputType::DEVICE),
      _dev(nullptr),
      _dev_wfn(nullptr),
      _file_wfn(nullptr)
{
}

bool whfa::Writer::open(const char *name, OutputType mode)
{
    std::lock_guard<std::mutex> lk(_mtx);
    close_unsafe();

    _mode = mode;

    if (!_ctxt->get_stream_spec(_spec))
    {
        set_state_stop(Worker::FORMATINVAL | Worker::CODECINVAL);
    }

    int rv = 0;
    switch (_mode)
    {
    case DEVICE:
        rv = open_dev(_dev, name, _spec);
        _dev_wfn = get_dev_writer(_spec);
    case FILE_RAW:
        rv = open_file_raw(_ofs, name, _spec);
        _file_wfn = get_file_writer(_spec);
        break;
    case FILE_WAV:
        rv = open_file_wav(_ofs, name, _spec);
        _file_wfn = get_file_writer(_spec);
        break;
    }
    if (rv != 0)
    {
        set_state_stop(rv);
        close_unsafe();
        return false;
    }
    return true;
}

bool whfa::Writer::close()
{
    std::lock_guard<std::mutex> lk(_mtx);
    return close_unsafe();
}

/**
 * whfa::Writer protected methods
 */

void whfa::Writer::execute_loop_body()
{
    switch (_mode)
    {
    case DEVICE:
        if (_dev == nullptr)
        {
            set_state_stop();
            return;
        }
        // snd_pcm_writei
        break;
    case FILE_RAW:
    case FILE_WAV:
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
        // both are now writing interleaved binary samples
        break;
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
        close_unsafe();
        return;
    }

    int rv;
    switch (_mode)
    {
    case DEVICE:
        rv = _dev_wfn(_dev, *frame, _spec);
        break;
    case FILE_RAW:
    case FILE_WAV:
        rv = _file_wfn(_ofs, *frame, _spec);
        break;
    }
    if (rv != 0)
    {
        set_state_pause(rv);
    }
}

bool whfa::Writer::close_unsafe()
{
    if (_dev != nullptr)
    {
        int rv = 0;
        rv |= snd_pcm_drain(_dev);
        rv |= snd_pcm_close(_dev);
        if (rv < 0)
        {
            set_state_pause(rv);
        }
    }
    if (_ofs.is_open())
    {
        _ofs.close();
    }
    _dev_wfn = nullptr;
    _file_wfn = nullptr;
    return false;
}
