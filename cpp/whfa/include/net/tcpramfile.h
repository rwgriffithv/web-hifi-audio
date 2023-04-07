/**
 * @file net/tcpramfile.h
 * @author Robert Griffith
 */
#pragma once

#include "net/tcpconnection.h"
#include "util/threader.h"

namespace whfa::net
{

    /**
     * @class whfa::net::TCPRAMFile
     * @brief threadsafe class for receiving remote file over TCP
     *
     * to be used as source of custom allocated AVIOContext for streaming
     * util::Threader mutex used to synchronize starting and stopping of
     * file using recv on open socket
     * own read mutex used to synchronize opening, closing, and reading of
     * socket
     * closing of socket will cause blocking or next recv calls to fail
     */
    class TCPRAMFile : public util::Threader
    {
    public:
        /// @brief default transfer block size in bytes
        static constexpr uint64_t DEF_BLOCKSZ = 1024;

        /**
         * @brief constructor
         *
         * @param blocksz size of bytes loaded into ram per loop iteration
         */
        TCPRAMFile(uint64_t blocksz = DEF_BLOCKSZ);

        /**
         * @brief destructor
         */
        virtual ~TCPRAMFile();

        /**
         * @brief open connection and exchange file size and block size
         *
         * @param addr IP address to connect to
         * @param port port to connect to
         * @return true if successful, check state and errno if false
         */
        bool open(const char *addr, uint16_t port);

        /**
         * @brief open connection and exchange file size and block size
         *
         * @param port port to listen and accept connection on
         * @return true if successful, check state and errno if false
         */
        bool open(uint16_t port);

        /**
         * @brief copy bytes from current read position into buffer, can block
         *
         * blocks if waiting to receive more expected data over network
         *
         * @param[out] buf byte buffer to populate
         * @param size number of bytes to read at most
         * @return number of bytes read, 0 if end of file (or no data)
         */
        uint64_t read(uint8_t *buf, uint64_t size);

        /**
         * @brief fseek and avio_seek equivalent, can block
         *
         * blocks if waiting to receive more expected data over network
         *
         * @param offset byte offset to move read position to
         * @param whence position from which offset is applied
         * @return true if successful, false if bad parameter values
         */
        bool seek(int64_t offset, int whence);

        /**
         * @brief close open connection
         */
        void close();

        /**
         * @brief get block size used in networked file transfer
         *
         * @return size of bytes loaded into ram per loop iteration
         */
        uint64_t get_blocksize();

        /**
         * @brief get file size
         *
         * @return file size in bytes
         */
        uint64_t get_filesize();

    protected:
        /**
         * @brief receive and write file data into buffer
         *
         * upon failure, pauses and sets error state without closing connection
         */
        void execute_loop_body() override;

        /// @brief tcp connection
        TCPConnection _conn;
        /// @brief max size per transfer in bytes
        uint64_t _blocksz;
        /// @brief file bytes
        uint8_t *_data;
        /// @brief file size
        uint64_t _filesz;

        /// @brief current file read byte position
        uint64_t _read_pos;
        /// @brief mutex for file read/open synchronization
        std::mutex _read_mtx;

        /// @brief current socket recv byte position
        uint64_t _recv_pos;
        /// @brief condition variable for notifying of recv updates
        std::condition_variable _recv_cond;
    };

}