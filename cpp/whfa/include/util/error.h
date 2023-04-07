/**
 * @file util/error.h
 * @author Robert Griffith
 */
#pragma once

#include <ostream>

namespace whfa::util
{
    
    /// @brief no error
    constexpr int ENONE = 0;

    /// @brief error code for failed established connection
    constexpr int ENET_CONNFAIL = 0x10;
    /// @brief error code for failed transmit
    constexpr int ENET_TXFAIL = 0x20;
    
    /// @brief error code for invalid codec
    constexpr int EPCM_CODECINVAL = 0x100;
    /// @brief error code for invalid format
    constexpr int EPCM_FORMATINVAL = 0x200;

    /**
     * @brief print error message to stderr
     *
     * @param error whfa::util error code to get message for
     */
    void print_error(int error);

    /**
     * @brief write error message to output stream
     *
     * @param os output stream to write to
     * @param error whfa::util error code to get message for
     */
    void stream_error(std::ostream &os, int error);

}