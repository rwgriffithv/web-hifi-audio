/**
 * @file tcpramfile.h
 * @author Robert Griffith
 */
#pragma once

#include "util/threader.h"

namespace whfa::net
{

    /**
     * @class TCPRAMFile
     * @brief threadsafe class for receiving remote file over TCP
     * 
     * to be used as source of custom allocated AVIOContext for streaming
     */
    class TCPRAMFile : public util::Threader
    {
    public:
        /// @brief default transfer block size in bytes
        static constexpr size_t DEF_BLOCKSZ = 1024;

        /**
         * @brief constructor
         *
         * @param blocksz size per transfer in bytes
         */
        TCPRAMFile(size_t blocksz = DEF_BLOCKSZ);

        /**
         * @brief destructor
         */
        virtual ~TCPRAMFile();

        /**
         * @brief open connection and exchange file size and block size
         *
         * @param addr IP address to connect to
         * @param port port to connect to
         * @return true if successful
         */
        bool open(const char *addr, uint16_t port);

        /**
         * @brief copy bytes from current read position into buffer, can block
         *
         * blocks if file is waiting for more data
         * (e.g. read pos + size >= write pos)
         *
         * @param[out] buf byte buffer to populate
         * @param size number of bytes to read at most
         * @return number of bytes read, EOF if end of file, negative errcode otherwise
         */
        int read(uint8_t *buf, size_t size);

        /**
         * @brief fseek and avio_seek equivalent
         *
         * @param offset byte offset to move read position to
         * @param whence position from which offset is applied
         * @return true if successful, sets error state upon failure
         */
        bool seek(int64_t offset, int whence);

        /**
         * @brief close open connection
         */
        void close();

    protected:
        /**
         * @brief receive file bytes from open connection
         *
         * upon failure, pauses and sets error state
         */
        void execute_loop_body() override;

        /// @brief mutex for handling socket synchronization
        std::mutex _filemtx;
        /// @brief socket file descriptor, -1 if not open
        int _fd;
        /// @brief size per transfer in bytes
        size_t _blocksz;
        /// @brief transfered bytes
        uint8_t *_data;
        /// @brief file size
        uint64_t _size;
        /// @brief byte position to read from
        size_t _pos_read;
        /// @brief byte position to write to
        size_t _pos_write;
    };

}