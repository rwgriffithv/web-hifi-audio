/**
 * @file pcm/player.cpp
 * @author Robert Griffith
 */
#include "pcm/player.h"
#include "util/error.h"

#include <array>

namespace wu = whfa::util;

namespace
{

    /// @brief number of libav formats supported for playback
    constexpr size_t __NUM_AVFMTS = 10;

    /// @brief convience alias for stream spec
    using WPCStreamSpec = whfa::pcm::Context::StreamSpec;
    /// @brief convenience alias for frame handler
    using WPFrameHandler = whfa::pcm::FrameHandler;
    /// @brief array type for libasound format mapping
    using SndFormatMap = std::array<snd_pcm_format_t, __NUM_AVFMTS>;

    /**
     * @brief compile time construction of sample format type map
     *
     * libav format -> libasound format (24 bit needs help)
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

    /// @brief libav format -> libasound format
    constexpr const SndFormatMap __SNDFMT_MAP = constSampleFormatMap();

    /**
     * @class DeviceWriter
     * @brief small class to handle efficient varitations of writing to device
     */
    class DeviceWriter : public WPFrameHandler
    {
    public:
        /**
         * @brief constructor
         *
         * @param dev libasound PCM device handle
         */
        DeviceWriter(snd_pcm_t *dev)
            : _dev(dev)
        {
        }

    protected:
        /// @brief libasound PCM device handle
        snd_pcm_t *_dev;
    };

    /**
     * @class DWInterleaved
     * @brief class for writing full frame of non-planar samples
     */
    class DWInterleaved : public DeviceWriter
    {
    public:
        /**
         * @brief constructor
         *
         * @param dev libasound PCM device handle
         * @param spec context stream specification
         */
        DWInterleaved(snd_pcm_t *dev, const WPCStreamSpec &spec)
            : DeviceWriter(dev),
              _ssz(av_get_bytes_per_sample(spec.format) * spec.channels)
        {
        }

        /**
         * @brief write all interleaved samples in frame to device
         *
         * @param frame libav frame to handle
         * @return 0 on success, error code on failure
         */
        int handle(const AVFrame &frame) override
        {
            int cnt = 0;
            while (cnt < frame.nb_samples)
            {
                const void *data = static_cast<const void *>(&(frame.extended_data[0][cnt * _ssz]));
                snd_pcm_uframes_t rv = snd_pcm_writei(_dev, data, frame.nb_samples - cnt);
                if (rv < 0)
                {
                    rv = snd_pcm_recover(_dev, rv, 0);
                }
                if (rv < 0)
                {
                    return rv;
                }
                cnt += rv;
            }
            return 0;
        }

    protected:
        /// @brief size of sample across all channels in bytes (_bw * channels)
        const int _ssz;
    };

    /**
     * @class DWPlanar
     * @brief class for writing full frame of planar samples
     */
    class DWPlanar : public DeviceWriter
    {
    public:
        /**
         * @brief constructor
         *
         * @param dev libasound PCM device handle
         * @param spec context stream specification
         */
        DWPlanar(snd_pcm_t *dev, const WPCStreamSpec &spec)
            : DeviceWriter(dev),
              _bw(av_get_bytes_per_sample(spec.format)),
              _pbuf(new void *[spec.channels])
        {
        }

        /**
         * @brief destructor, free allocated buffer of planar channel pointers
         */
        ~DWPlanar()
        {
            delete _pbuf;
        }

        /**
         * @brief write all planar samples in frame to device
         *
         * @param frame libav frame to handle
         * @return 0 on success, error code on failure
         */
        int handle(const AVFrame &frame) override
        {
            int cnt = 0;
            while (cnt < frame.nb_samples)
            {
                for (int i = 0; i < frame.channels; ++i)
                {
                    _pbuf[i] = static_cast<void *>(&(frame.extended_data[i][cnt * _bw]));
                }
                snd_pcm_uframes_t rv = snd_pcm_writen(_dev, _pbuf, frame.nb_samples - cnt);
                if (rv < 0)
                {
                    rv = snd_pcm_recover(_dev, rv, 0);
                }
                if (rv < 0)
                {
                    return rv;
                }
                cnt += rv;
            }
            return 0;
        }

    protected:
        /// @brief bytewidth of individual channel sample
        const int _bw;
        /// @brief buffer of void pointers to manipulate for reading planar data
        void **_pbuf;
    };

    /**
     * @brief configure device according to context stream specification
     *
     * @param[out] dev libasound pcm device handle
     * @param spec context stream specification
     * @param resample enable/disable libasound resampling
     * @param latency_us latency for libasound playback in microseconds
     * @return 0 if successful, error code otherwise
     */
    int configure_dev(snd_pcm_t *&dev, const WPCStreamSpec &spec, bool resample, unsigned int latency_us)
    {
        const size_t fmt_idx = static_cast<size_t>(spec.format);
        if (fmt_idx >= __SNDFMT_MAP.size())
        {
            // unsupported format
            return wu::EPCM_CODECINVAL;
        }
        snd_pcm_format_t format = __SNDFMT_MAP[fmt_idx];
        // special case for 24 bit samples packed in 32 bit widths (handled by libasound without subsampling)
        if (format == SND_PCM_FORMAT_S32 && spec.bitdepth == 24)
        {
            format = SND_PCM_FORMAT_S24;
        }
        // do not handle other subsample cases
        const int bw = av_get_bytes_per_sample(spec.format) << 3;
        if (format != SND_PCM_FORMAT_S24 && spec.bitdepth < bw)
        {
            // unsupported subsample format
            return wu::EPCM_CODECINVAL;
        }
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        const snd_pcm_access_t access = planar
                                            ? SND_PCM_ACCESS_RW_NONINTERLEAVED
                                            : SND_PCM_ACCESS_RW_INTERLEAVED;
        snd_pcm_drain(dev);
        return snd_pcm_set_params(dev,
                                  format,
                                  access,
                                  spec.channels,
                                  spec.rate,
                                  resample ? 1 : 0,
                                  latency_us);
    }

    /**
     * @brief select device writer according to stream specification
     *
     * @param dev libasound PCM device handle
     * @param spec stream specification
     * @return device writer, nullptr on invalid/unsupported spec
     */
    WPFrameHandler *get_dev_writer(snd_pcm_t *dev, const WPCStreamSpec &spec)
    {
        const bool planar = av_sample_fmt_is_planar(spec.format) == 1;
        WPFrameHandler *dw = nullptr;
        if (planar)
        {
            dw = static_cast<WPFrameHandler *>(new DWPlanar(dev, spec));
        }
        else
        {
            dw = static_cast<WPFrameHandler *>(new DWInterleaved(dev, spec));
        }
        return dw;
    }

}

namespace whfa::pcm
{

    /**
     * whfa::pcm::Player public methods
     */

    Player::Player(Context &context)
        : Worker(context),
          _dev(nullptr),
          _writer(nullptr)
    {
    }

    Player::~Player()
    {
        close();
    }

    bool Player::open(const char *devname)
    {
        std::lock_guard<std::mutex> lk(_mtx);

        if (_dev != nullptr)
        {
            snd_pcm_drain(_dev);
            snd_pcm_close(_dev);
        }

        const int rv = snd_pcm_open(&_dev, devname, SND_PCM_STREAM_PLAYBACK, 0);
        const bool err = rv != 0;
        if (err)
        {
            set_state_stop(rv);
            snd_pcm_close(_dev);
            _dev = nullptr;
        }
        return !err;
    }

    bool Player::configure(bool resample, unsigned int latency_us)
    {
        std::lock_guard<std::mutex> lk(_mtx);

        if (_dev == nullptr)
        {
            return false;
        }

        if (!_ctxt->get_stream_spec(_spec))
        {
            set_state_stop(wu::EPCM_FORMATINVAL | wu::EPCM_CODECINVAL);
            return false;
        }

        if (_writer != nullptr)
        {
            delete _writer;
        }
        _writer = get_dev_writer(_dev, _spec);

        const int rv = configure_dev(_dev, _spec, resample, latency_us);
        if (rv != 0)
        {
            set_state_stop(rv);
            return false;
        }

        return true;
    }

    void Player::close()
    {
        std::lock_guard<std::mutex> lk(_mtx);

        if (_writer != nullptr)
        {
            delete _writer;
            _writer = nullptr;
        }

        int rv = 0;
        if (_dev != nullptr)
        {
            rv |= snd_pcm_drain(_dev);
            rv |= snd_pcm_close(_dev);
            _dev = nullptr;
        }
        set_state_stop(rv);
    }

    /**
     * whfa::pcm::Player protected methods
     */

    void Player::execute_loop_body()
    {

        if (_dev == nullptr)
        {
            set_state_stop();
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
            // EOF, drain and stop
            set_state_stop(snd_pcm_drain(_dev));
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