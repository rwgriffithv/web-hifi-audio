/**
 * @file writer.cpp
 * @author Robert Griffith
 *
 * @todo consider exposing device resampling and latency in public methods
 * @todo use map (array) of function pointers to handle writing different formats
 * @todo map indexed by spec.format
 */
#include "pcm/writer.h"

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

    using WPCStreamSpec = whfa::pcm::Context::StreamSpec;

    /// @brief array type for asoundlib format mapping
    using SndFormatMap = std::array<snd_pcm_format_t, __NUM_AVFMTS>;
    /// @brief array type for wav format mapping
    using WavFormatMap = std::array<uint16_t, __NUM_AVFMTS>;

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

    /// @brief libav format -> asoundlib format
    constexpr const SndFormatMap __SNDFMT_MAP = constSampleFormatMap();
    /// @brief libav format -> wav chunk format tag
    constexpr const WavFormatMap __WAVFMT_MAP = constWavFormatMap();

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
    int open_dev(snd_pcm_t *&handle, const char *name, const WPCStreamSpec &spec)
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
    int open_file_raw(std::ofstream &ofs, const char *name, const WPCStreamSpec &spec)
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
    int open_file_wav(std::ofstream &ofs, const char *name, const WPCStreamSpec &spec)
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
     * @brief write device using interleaved full samples (bitdepth == primitive bitwidth)
     *
     * @param handle asoundlib pcm device handle
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data (unused)
     * @return 0 if successful, error code otherwise
     */
    int write_dev_fs(snd_pcm_t *handle, const AVFrame &frame, const WPCStreamSpec &spec, [[maybe_unused]] void **pbuf)
    {
        const int ssz = spec.bitdepth >> 3;
        int cnt = 0;
        while (cnt < frame.nb_samples)
        {
            const void *data = static_cast<const void *>(&(frame.extended_data[0][cnt * ssz]));
            snd_pcm_uframes_t rv = snd_pcm_writei(handle, data, frame.nb_samples - cnt);
            if (rv < 0)
            {
                rv = snd_pcm_recover(handle, rv, 0);
            }
            if (rv < 0)
            {
                return rv;
            }
            cnt += rv;
        }
        return 0;
    }

    /**
     * @brief write device using planar full samples (bitdepth == primitive bitwidth)
     *
     * @param handle asoundlib pcm device handle
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_dev_fs_p(snd_pcm_t *handle, const AVFrame &frame, const WPCStreamSpec &spec, void **pbuf)
    {
        const int ssz = spec.bitdepth >> 3;
        int cnt = 0;
        while (cnt < frame.nb_samples)
        {
            for (int i = 0; i < frame.channels; ++i)
            {
                pbuf[i] = static_cast<void *>(&(frame.extended_data[i][cnt * ssz]));
            }
            snd_pcm_uframes_t rv = snd_pcm_writen(handle, pbuf, frame.nb_samples - cnt);
            if (rv < 0)
            {
                rv = snd_pcm_recover(handle, rv, 0);
            }
            if (rv < 0)
            {
                return rv;
            }
            cnt += rv;
        }
        return 0;
    }

    /**
     * @brief write device using interleaved sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param handle asoundlib pcm device handle
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data (unused)
     * @return 0 if successful, error code otherwise
     */
    int write_dev_ss(snd_pcm_t *handle, const AVFrame &frame, const WPCStreamSpec &spec, [[maybe_unused]] void **pbuf)
    {
        const int ssz = av_get_bytes_per_sample(spec.format);
        int cnt = 0;
        while (cnt < frame.nb_samples)
        {
            const void *data = static_cast<const void *>(&(frame.extended_data[0][cnt * ssz]));
            snd_pcm_uframes_t rv = snd_pcm_writei(handle, data, 1);
            if (rv < 0)
            {
                rv = snd_pcm_recover(handle, rv, 0);
            }
            if (rv < 0)
            {
                return rv;
            }
            cnt += rv;
        }
        return 0;
    }

    /**
     * @brief write device using planar sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param handle asoundlib pcm device handle
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_dev_ss_p(snd_pcm_t *handle, const AVFrame &frame, const WPCStreamSpec &spec, void **pbuf)
    {
        const int ssz = av_get_bytes_per_sample(spec.format);
        int cnt = 0;
        while (cnt < frame.nb_samples)
        {
            for (int i = 0; i < frame.channels; ++i)
            {
                pbuf[i] = static_cast<void *>(&(frame.extended_data[i][cnt * ssz]));
            }
            snd_pcm_uframes_t rv = snd_pcm_writen(handle, pbuf, 1);
            if (rv < 0)
            {
                rv = snd_pcm_recover(handle, rv, 0);
            }
            if (rv < 0)
            {
                return rv;
            }
            cnt += rv;
        }
        return 0;
    }

    /**
     * @brief write file using planar sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param ofs output file stream
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_file_fs(std::ofstream &ofs, const AVFrame &frame, const WPCStreamSpec &spec)
    {
        const std::streamsize ssz = spec.bitdepth >> 3;
        const std::streamsize framesz = ssz * frame.nb_samples * frame.channels;
        ofs.write(reinterpret_cast<const char *>(frame.extended_data[0]), framesz);
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief write file using planar sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param ofs output file stream
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_file_fs_p(std::ofstream &ofs, const AVFrame &frame, const WPCStreamSpec &spec)
    {
        const int ssz = spec.bitdepth >> 3;
        const int planesz = ssz * frame.nb_samples;
        for (int i = 0; i < planesz; i += ssz)
        {
            for (int c = 0; c < frame.channels; ++c)
            {
                ofs.write(reinterpret_cast<const char *>(&(frame.extended_data[c][i])), ssz);
            }
        }
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief write file using planar sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param ofs output file stream
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_file_ss(std::ofstream &ofs, const AVFrame &frame, const WPCStreamSpec &spec)
    {
        const int ssz = av_get_bytes_per_sample(spec.format);
        const int planesz = ssz * frame.nb_samples;
        const std::streamsize subsz = spec.bitdepth >> 3;
        for (int i = 0; i < planesz; i += ssz)
        {
            const char *data = reinterpret_cast<const char *>(&(frame.extended_data[0][i]));
            ofs.write(data, subsz);
        }
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief write file using planar sub-samples (bitdepth <= primitive bitwidth)
     *
     * @param ofs output file stream
     * @param frame libav frame to write
     * @param spec context stream specification
     * @param pbuf allocated void* buffer to use for accessing planar data
     * @return 0 if successful, error code otherwise
     */
    int write_file_ss_p(std::ofstream &ofs, const AVFrame &frame, const WPCStreamSpec &spec)
    {
        const int ssz = av_get_bytes_per_sample(spec.format);
        const int planesz = ssz * frame.nb_samples;
        const std::streamsize subsz = spec.bitdepth >> 3;
        for (int i = 0; i < planesz; i += ssz)
        {
            for (int c = 0; c < frame.channels; ++c)
            {
                ofs.write(reinterpret_cast<const char *>(&(frame.extended_data[c][i])), subsz);
            }
        }
        return ofs ? 0 : ofs.rdstate();
    }

    /**
     * @brief select device writer according to stream specification
     *
     * @param spec stream specification
     * @param[out] pbuf planar data buffer, allocated if needed
     * @return device writer, nullptr on invalid/unsupported spec
     */
    whfa::pcm::Writer::DeviceWriter get_dev_writer(const WPCStreamSpec &spec, void **&pbuf)
    {
        if (pbuf != nullptr)
        {
            delete pbuf;
        }
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        const int bw = av_get_bytes_per_sample(spec.format) << 3;
        const bool subsample = spec.bitdepth < bw;
        if (planar)
        {
            // planar buffer required by planar device writing functions
            pbuf = new void *[spec.channels];
            return subsample ? write_dev_ss_p : write_dev_fs_p;
        }
        pbuf = nullptr;
        return subsample ? write_dev_ss : write_dev_fs;
    }

    /**
     * @brief select file writer according to stream specification
     *
     * @param spec stream specification
     * @return file writer, nullptr on invalid/unsupported spec
     */
    whfa::pcm::Writer::FileWriter get_file_writer(const WPCStreamSpec &spec)
    {
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        const int bw = av_get_bytes_per_sample(spec.format) << 3;
        const bool subsample = spec.bitdepth < bw;
        if (planar)
        {
            return subsample ? write_file_ss_p : write_file_fs_p;
        }
        return subsample ? write_file_ss : write_file_fs;
    }
}

namespace whfa::pcm
{

    /**
     * whfa::pcm::Writer public methods
     */

    Writer::Writer(Context &context)
        : Worker(context),
          _mode(OutputType::DEVICE),
          _dev(nullptr),
          _pbuf(nullptr),
          _dev_wfn(nullptr),
          _file_wfn(nullptr)
    {
    }

    Writer::~Writer()
    {
        close();
    }

    bool Writer::open(const char *name, OutputType mode)
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
            _dev_wfn = get_dev_writer(_spec, _pbuf);
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

    bool Writer::close()
    {
        std::lock_guard<std::mutex> lk(_mtx);
        return close_unsafe();
    }

    /**
     * whfa::pcm::Writer protected methods
     */

    void Writer::execute_loop_body()
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
            rv = _dev_wfn(_dev, *frame, _spec, _pbuf);
            break;
        case FILE_RAW:
        case FILE_WAV:
            rv = _file_wfn(_ofs, *frame, _spec);
            break;
        }
        set_state_timestamp(frame->pts);
        av_frame_free(&frame);
        if (rv != 0)
        {
            set_state_pause(rv);
        }
    }

    bool Writer::close_unsafe()
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
        if (_pbuf != nullptr)
        {
            delete _pbuf;
        }
        _dev_wfn = nullptr;
        _file_wfn = nullptr;
        return false;
    }

}