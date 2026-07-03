/*****************************************************************************
 * process_runner.hpp
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_DOWNLOADER_PROCESS_RUNNER_HPP
#define VLC_DOWNLOADER_PROCESS_RUNNER_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>

struct vlc_object_t;

namespace vlc {
namespace downloader {

class CancellationToken;

/**
 * @brief RAII wrapper around VLC's vlc_spawnp() for subprocess management.
 *
 * Manages the lifecycle of a child process: spawn, read output, wait,
 * and kill on destruction. Designed after the pattern in modules/demux/ytdl.c.
 *
 * Two usage modes:
 *   1. Synchronous: run() spawns, captures all output, waits, and returns Result.
 *   2. Staged:      start() → readStdout()/readStderr() → wait() for line-by-line.
 */
class ProcessRunner
{
public:
    /** Aggregated result of a completed process. */
    struct Result
    {
        int exitStatus = -1;   /**< Process exit code */
        std::string stdOut;    /**< Captured stdout content */
        std::string stdErr;    /**< Captured stderr content */
        bool succeeded = false; /**< Exit code 0 */
    };

    /** Options for spawning a process. */
    struct Options
    {
        const char* executablePath = nullptr;  /**< Path to executable (e.g., yt-dlp path) */
        std::vector<const char*> args;         /**< Argument list, null-terminated (argv style) */
        bool captureStdout = true;             /**< Capture stdout (vs /dev/null) */
        bool captureStderr = true;             /**< Capture stderr (vs /dev/null) */
        vlc_object_t* logger = nullptr;        /**< VLC object for logging (msg_*) */
    };

    /**
     * @brief Run a process synchronously and capture all output.
     * This is the simplest usage: spawn → read → wait → return Result.
     *
     * @param opts    Process options.
     * @param token   Optional cancellation token. If cancelled during execution,
     *                the process is killed and Result::succeeded is false.
     * @return Aggregated Result struct.
     */
    static Result run(const Options& opts, CancellationToken* token = nullptr);

    ProcessRunner() = default;
    ~ProcessRunner();

    /** Non-copyable, movable */
    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;
    ProcessRunner(ProcessRunner&& other) noexcept;
    ProcessRunner& operator=(ProcessRunner&& other) noexcept;

    /**
     * @brief Start a child process.
     * @param opts Process options (executable, args, pipe config).
     * @return true on success, false on error (errno set, or msg logged).
     */
    bool start(const Options& opts);

    /**
     * @brief Read all remaining data from stdout.
     * Blocks until the write end of the pipe is closed (i.e., process exits).
     * @return String containing all stdout data.
     */
    std::string readAllStdout();

    /**
     * @brief Read one line from stderr.
     * Blocks until a newline is received or the pipe closes.
     * Returns empty string on EOF.
     *
     * @param token  Optional cancellation token. When set and cancelled,
     *               the method will return empty on the next EAGAIN
     *               (no data available), allowing the caller to abort.
     */
    std::string readStderrLine(CancellationToken* token = nullptr);

    /**
     * @brief Wait for the process to finish and collect exit status.
     * @return The process exit code. Must be called to release system resources.
     */
    int wait();

    /**
     * @brief Terminate the child process.
     * Safe to call from any thread (e.g., UI thread to cancel a download).
     */
    void cancel();

    /** @brief Check if the process is still running. */
    bool isRunning() const { return m_running; }

    /** @brief The process ID, or -1 if not started. */
    pid_t pid() const { return m_pid; }

private:
    /** Read into an internal buffer from a pipe fd. Returns bytes read. */
    static ssize_t readFd(int fd, std::string& buffer, size_t chunkSize = 4096);

    pid_t m_pid = -1;
    int m_stdoutFd = -1;
    int m_stderrFd = -1;
    bool m_running = false;
    bool m_started = false;

    /* Buffers for partial reads */
    std::string m_stdoutBuf;
    std::string m_stderrBuf;

    /* Staging options for staged mode */
    Options m_opts;
};

} // namespace downloader
} // namespace vlc

#endif // VLC_DOWNLOADER_PROCESS_RUNNER_HPP
