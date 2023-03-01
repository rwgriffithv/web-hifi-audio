/**
 * @file player.h
 * @author Robert Griffith
 */
#pragma once

#include "pcm/context.h"

#include <alsa/asoundlib.h>

namespace whfa::pcm
{

    /**
     * @class whfa::pcm::Player
     * @brief class for parallel playing of PCM audio frames
     *
     * context worker class to abstract forwarding libav frames to files and devices
     * will write to only one sink at a time (potentially changed later)
     * a context should only have one writer/player, as it consumes frames destructively from the queue
     *
     * @todo: utilize queue pop timeouts? period determined from sample rate?
     * @todo: try creating common base class for Player and Writer, multithreaded processing of each poppped frame
     */
    class Player : public Context::Worker
    {
    public:
        /// @brief default enable/disable libasound resampling
        static constexpr bool DEF_RESAMPLE = false;
        /// @brief default latency for libasound playback in microseconds
        static constexpr unsigned int DEF_LATENCY_US = 500000;
        
        /**
         * @class whfa::pcm::Player::DeviceWriter
         * @brief small class to handle efficient varitations of writing to device
         */
        class DeviceWriter
        {
        public:
            /**
             * @brief constructor
             *
             * @param dev libasound PCM device handle
             * @param spec context stream specification
             */
            DeviceWriter(snd_pcm_t *dev, const Context::StreamSpec &spec);

            /**
             * @brief destructor
             */
            virtual ~DeviceWriter();

            /**
             * @brief write frame to device
             *
             * @param frame libav frame to write
             * @return 0 on success, error code on failure
             */
            virtual int write(const AVFrame &frame) = 0;

        protected:
            /// @brief libasound PCM device handle
            snd_pcm_t *_dev;
        };

        /**
         * @brief constructor
         *
         * @param context threadsafe audio context to access
         */
        Player(Context &context);

        /**
         * @brief destructor to ensure device closure
         */
        ~Player();

        /**
         * @brief open
         *
         * sets mode, open connection, sets spec, and sets writer
         * only one device will be open at a time
         * closing and opening of same device can be asynchronous and may fail
         * should open one device for as long as possible and use configure() for new audio
         *
         * @param devname the name of the device to open
         * @return true on success, false otherwise
         */
        bool open(const char *devname);

        /**
         * @brief configure open device using current Context
         *
         * drains current playback and sets hardware and software parameters for device
         *
         * @param resample enable/disable libasound resampling
         * @param latency_us latency for libasound playback in microseconds
         * @return false if failure or no device opened
         */
        bool configure(bool resample = DEF_RESAMPLE, unsigned int latency_us = DEF_LATENCY_US);

        /**
         * @brief close open device
         */
        void close();

    protected:
        /**
         * @brief write queued frames to opened destination
         *
         * upon failure, pauses and sets error state without altering context or closing
         */
        void execute_loop_body() override;

        /// @brief libasound PCM device handle
        snd_pcm_t *_dev;
        /// @brief context stream specification
        Context::StreamSpec _spec;
        /// @brief class to write to device with
        DeviceWriter *_writer;
    };

}