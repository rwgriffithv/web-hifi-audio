/**
 * @file util/error.h
 * @author Robert Griffith
 */
#pragma once

#include <ostream>

namespace whfa::util
{
    /**
     * @brief constexpr creation of tag to be a component of whfa error
     *
     * @param a ls byte
     * @param b 2nd ls byte
     * @param c 2nd ms byte
     * @param d ms byte
     * @return int dcba
     */
    constexpr int make_error_tag(int a, int b, int c, int d)
    {
        return a | (b << 8) | (c << 16) | (d << 24);
    }

    /**
     * @brief constexpr creation of whfa error code from 4 ints
     *
     * @param a int
     * @param b int
     * @param c int
     * @param d int
     * @return whfa error code
     */
    constexpr int make_error(int a, int b, int c, int d)
    {
        return make_error_tag(a, b, c, d) ^ make_error_tag('W', 'H', 'F', 'A');
    }

    /// @brief error code for no error
    constexpr int ENONE = 0;
    /// @brief error code for invalid libav format
    constexpr int EINVFORMAT = make_error('I', 'F', 'M', 'T');
    /// @brief error code for invalid libav codec
    constexpr int EINVCODEC = make_error('I', 'C', 'D', 'C');
    /// @brief error code for invalid libav stream generally
    constexpr int EINVSTREAM = make_error('I', 'S', 'T', 'M');

    /**
     * @brief print error string to stderr
     *
     * @param error whfa::util error code to get string for
     */
    void print_error(int error);

    /**
     * @brief write error string to output stream
     *
     * @param os output stream to write to
     * @param error whfa::util error code to get string for
     */
    void stream_error(std::ostream &os, int error);

}