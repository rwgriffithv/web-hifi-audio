/**
 * @file test/pcm.h
 * @author Robert Griffith
 */
#pragma once

namespace whfa::test
{

    /**
     * @brief test playing audio file
     *
     * @param url url to file to play
     * @param dev ALSA device to play to
     */
    void test_play(const char *url, const char *dev);

    /**
     * @brief test writing audio file to raw PCM
     *
     * @param url url to file to read
     */
    void test_write_raw(const char *url);

    /**
     * @brief test writing audio file to WAV
     *
     * @param url url to file to read
     */
    void test_write_wav(const char *url);

}