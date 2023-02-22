/**
 * @file writer.cpp
 * @author Robert Griffith
 *
 * @todo: consider exposing device resampling and latency in public methods
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
    constexpr size_t __WAV_HDRSZ = 44;
    /// @brief number of supported libav formats
    constexpr size_t __NUM_AVFMTS = 10;

    using format_map = std::array<snd_pcm_format_t, __NUM_AVFMTS>;
    using access_map = std::array<snd_pcm_access_t, __NUM_AVFMTS>;

    /**
     * @brief compile time construction of sample format type map
     *
     * libav format -> asoundlib format
     */
    constexpr format_map constSampleFormatMap()
    {
        format_map map = {SND_PCM_FORMAT_UNKNOWN};
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
     * @brief compile time construction of sample access type map
     *
     * libav format -> asoundlib access
     */
    constexpr access_map constSampleAccessMap()
    {
        access_map map = {SND_PCM_ACCESS_RW_INTERLEAVED};
        // planar values only
        // 8 bit
        map[AV_SAMPLE_FMT_U8P] = SND_PCM_ACCESS_RW_NONINTERLEAVED;
        // 16 bit
        map[AV_SAMPLE_FMT_S16P] = SND_PCM_ACCESS_RW_NONINTERLEAVED;
        // 24 or 32 bit
        map[AV_SAMPLE_FMT_S32P] = SND_PCM_ACCESS_RW_NONINTERLEAVED;
        // float
        map[AV_SAMPLE_FMT_FLTP] = SND_PCM_ACCESS_RW_NONINTERLEAVED;
        // double
        map[AV_SAMPLE_FMT_DBLP] = SND_PCM_ACCESS_RW_NONINTERLEAVED;
        // 64 bit not supported
        return map;
    }

    /// @brief libav format -> asoundlib format map
    constexpr const format_map __FMT_MAP = constSampleFormatMap();
    /// @brief libav format -> asoundlib access map
    constexpr const access_map __ACC_MAP = constSampleAccessMap();

    /**
     * @brief write 2 byte value to output file stream
     *
     * @param ofs open output file stream
     * @param u 2 byte value to write
     */
    inline void write_2(std::ofstream &ofs, uint16_t u)
    {
        ofs.write(reinterpret_cast<char *>(&u), 2);
    }

    /**
     * @brief write 4 byte value to output file stream
     *
     * @param ofs open output file stream
     * @param u 4 byte value to write
     */
    inline void write_4(std::ofstream &ofs, uint32_t u)
    {
        ofs.write(reinterpret_cast<char *>(&u), 4);
    }

    /**
     * @brief open device according to context sample specification
     *
     * @param[out] handle asoundlib pcm device handle
     * @param name device name
     * @param spec context sample specification
     * @return 0 if successful, error code otherwise
     */
    int open_dev(snd_pcm_t *&handle, const char *name, const whfa::Context::SampleSpec &spec)
    {
        int rv;
        if ((rv = snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        {
            return rv;
        }

        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __FMT_MAP.size())
        {
            // unsupported format
            return -1;
        }

        snd_pcm_format_t format = __FMT_MAP[fmt_idx];
        snd_pcm_access_t access = __ACC_MAP[fmt_idx];

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
     * @param spec context sample specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_raw(std::ofstream &ofs, const char *name, const whfa::Context::SampleSpec &spec)
    {
        ofs.open(name, std::ios::out | std::ios::binary);
        if (!ofs.is_open())
        {
            return ofs.rdstate();
        }
        std::string name_md(name);
        name_md.append(__METADATA_SFX);
        std::ofstream ofs_md(name_md);
        if (!ofs_md.is_open())
        {
            return ofs_md.rdstate();
        }
        ofs_md << ".format = " << spec.format << std::endl;
        ofs_md << ".bitdepth = " << spec.bitdepth << std::endl;
        ofs_md << ".rate = " << spec.rate << std::endl;
        ofs_md << ".channels = " << spec.channels << std::endl;
        ofs_md.close();
        return 0;
    }

    /**
     * @brief open WAV file and write header (total file size filled in later)
     *
     * @param ofs output file stream to open and write to
     * @param name file location to open
     * @param spec context sample specification
     * @return 0 if successful, error coder otherwise
     */
    int open_file_wav(std::ofstream &ofs, const char *name, const whfa::Context::SampleSpec &spec)
    {
        ofs.open(name, std::ios::out | std::ios::binary);
        if (!ofs.is_open())
        {
            return ofs.rdstate();
        }
        uint32_t u4;
        uint16_t u2;
        ofs.write("RIFF", 4);
        u4 = 0;
        write_4(ofs, u4);
        ofs.write("WAVE", 4);
        ofs.write("fmt\0", 4);
        u4 = 16;
        write_4(ofs, u4);
        u2 = 1;
        write_2(ofs, u2);
        u2 = static_cast<uint16_t>(spec.channels);
        write_2(ofs, u2);
        u4 = static_cast<uint32_t>(spec.rate);
        write_4(ofs, u4);
        const int channel_bytedepth = spec.channels * spec.bitdepth >> 3;
        u4 = static_cast<uint32_t>(spec.rate * channel_bytedepth);
        write_4(ofs, u4);
        u2 = static_cast<uint16_t>(channel_bytedepth);
        write_2(ofs, u2);
        u2 = static_cast<uint16_t>(spec.bitdepth);
        write_2(ofs, u2);
        return 0;
    }
}

/**
 * whfa::Writer public methods
 */

whfa::Writer::Writer(whfa::Context &context)
    : Worker(context),
      _mode(OutputType::DEVICE),
      _dev(nullptr)
{
}

bool whfa::Writer::open(const char *name, OutputType mode)
{
    std::lock_guard<std::mutex> lk(_mtx);
    close_unsafe();

    Context::SampleSpec spec;
    if (!_ctxt->get_sample_spec(spec))
    {
        set_state_stop();
        set_state_error(Worker::CODECINVAL);
    }

    _mode = mode;
    int rv = 0;
    switch (_mode)
    {
    case DEVICE:
        rv = open_dev(_dev, name, spec);
    case FILE_RAW:
        rv = open_file_raw(_ofs, name, spec);
        break;
    case FILE_WAV:
        rv = open_file_wav(_ofs, name, spec);
        _fsize = __WAV_HDRSZ;
        break;
    }
    if (rv != 0)
    {
        close_unsafe();
        set_state_stop();
        set_state_error(rv);
    }
    return false;
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
            set_state_pause();
            set_state_error(rv);
        }
    }
    if (_ofs.is_open())
    {
        _ofs.close();
    }
    _fsize = 0;
    return false;
}
