/*****************************************************************************
 * process_runner.cpp
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

#include "process_runner.hpp"
#include "cancellation_token.hpp"

#include <vlc_common.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifdef _WIN32
# include <windows.h>
/* Wrap TerminateProcess to match POSIX kill() signature */
static int vlc_kill(pid_t pid, int signum)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h == NULL) {
        errno = ESRCH;
        return -1;
    }
    BOOL res;
    if (signum > 0)
        res = TerminateProcess(h, (UINT)signum);
    else
        res = TRUE;
    CloseHandle(h);
    return res ? 0 : -1;
}
# define kill(p, s) vlc_kill(p, s)
#endif

namespace vlc {
namespace downloader {

/* static */
ProcessRunner::Result ProcessRunner::run(const Options& opts, CancellationToken* token)
{
    ProcessRunner runner;
    if (!runner.start(opts))
        return Result{};

    Result result = {};

    /* Read stdout */
    if (opts.captureStdout)
        result.stdOut = runner.readAllStdout();

    /* Read all stderr */
    if (opts.captureStderr)
    {
        char buf[4096];
        ssize_t n;
        while ((n = read(runner.m_stderrFd, buf, sizeof(buf))) > 0)
        {
            result.stdErr.append(buf, static_cast<size_t>(n));
            if (token && token->isCancelled())
            {
                runner.cancel();
                break;
            }
        }
    }

    result.exitStatus = runner.wait();
    result.succeeded = (result.exitStatus == 0);
    return result;
}

ProcessRunner::~ProcessRunner()
{
    if (m_running)
    {
        cancel();
        wait();
    }
    if (m_stdoutFd >= 0)
        close(m_stdoutFd);
    if (m_stderrFd >= 0)
        close(m_stderrFd);
}

ProcessRunner::ProcessRunner(ProcessRunner&& other) noexcept
    : m_pid(other.m_pid)
    , m_stdoutFd(other.m_stdoutFd)
    , m_stderrFd(other.m_stderrFd)
    , m_running(other.m_running)
    , m_started(other.m_started)
    , m_stdoutBuf(std::move(other.m_stdoutBuf))
    , m_stderrBuf(std::move(other.m_stderrBuf))
    , m_opts(std::move(other.m_opts))
{
    other.m_pid = -1;
    other.m_stdoutFd = -1;
    other.m_stderrFd = -1;
    other.m_running = false;
    other.m_started = false;
}

ProcessRunner& ProcessRunner::operator=(ProcessRunner&& other) noexcept
{
    if (this != &other)
    {
        if (m_running)
        {
            cancel();
            wait();
        }
        if (m_stdoutFd >= 0) close(m_stdoutFd);
        if (m_stderrFd >= 0) close(m_stderrFd);

        m_pid = other.m_pid;
        m_stdoutFd = other.m_stdoutFd;
        m_stderrFd = other.m_stderrFd;
        m_running = other.m_running;
        m_started = other.m_started;
        m_stdoutBuf = std::move(other.m_stdoutBuf);
        m_stderrBuf = std::move(other.m_stderrBuf);
        m_opts = std::move(other.m_opts);

        other.m_pid = -1;
        other.m_stdoutFd = -1;
        other.m_stderrFd = -1;
        other.m_running = false;
        other.m_started = false;
    }
    return *this;
}

bool ProcessRunner::start(const Options& opts)
{
    if (m_started)
        return false;

    m_opts = opts;

    /* Create pipes using standard POSIX pipe() */
    int stdoutPipe[2] = { -1, -1 };
    int stderrPipe[2] = { -1, -1 };

    if (opts.captureStdout && pipe(stdoutPipe) != 0)
    {
        if (opts.logger)
            msg_Err(opts.logger, "Failed to create stdout pipe: %s", strerror(errno));
        return false;
    }

    if (opts.captureStderr && pipe(stderrPipe) != 0)
    {
        if (opts.logger)
            msg_Err(opts.logger, "Failed to create stderr pipe: %s", strerror(errno));
        if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
        return false;
    }

    /* Set the read ends to non-blocking so we can check for cancellation */
    if (opts.captureStdout)
    {
        int flags = fcntl(stdoutPipe[0], F_GETFL, 0);
        fcntl(stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);
    }
    if (opts.captureStderr)
    {
        int flags = fcntl(stderrPipe[0], F_GETFL, 0);
        fcntl(stderrPipe[0], F_SETFL, flags | O_NONBLOCK);
    }

    /* Build argv */
    std::vector<const char*> argv;
    argv.push_back(opts.executablePath);
    for (auto arg : opts.args)
        argv.push_back(arg);
    argv.push_back(nullptr);

    if (opts.logger)
        msg_Dbg(opts.logger, "Spawning: %s", opts.executablePath);

    /* Fork the child process */
    pid_t pid = fork();

    if (pid == -1)
    {
        /* Fork failed */
        if (opts.logger)
            msg_Err(opts.logger, "Failed to fork: %s", strerror(errno));
        if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
        if (stderrPipe[0] >= 0) close(stderrPipe[0]);
        if (stderrPipe[1] >= 0) close(stderrPipe[1]);
        return false;
    }

    if (pid == 0)
    {
        /* ── Child process ── */

        /* Redirect stdin to /dev/null */
        int devNull = open("/dev/null", O_RDONLY);
        if (devNull >= 0) {
            dup2(devNull, STDIN_FILENO);
            close(devNull);
        }

        /* Redirect stdout to pipe */
        if (opts.captureStdout) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
        }

        /* Redirect stderr to pipe */
        if (opts.captureStderr) {
            dup2(stderrPipe[1], STDERR_FILENO);
        }

        /* Close all pipe fds in the child */
        if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
        if (stderrPipe[0] >= 0) close(stderrPipe[0]);
        if (stderrPipe[1] >= 0) close(stderrPipe[1]);

        /* Execute the program (searches PATH) */
        execvp(opts.executablePath, const_cast<char* const*>(argv.data()));

        /* If execvp returns, it failed */
        _exit(127);
    }

    /* ── Parent process ── */

    m_pid = pid;

    /* Close write ends in parent */
    if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
    if (stderrPipe[1] >= 0) close(stderrPipe[1]);

    m_stdoutFd = stdoutPipe[0];
    m_stderrFd = stderrPipe[0];
    m_running = true;
    m_started = true;

    if (opts.logger)
        msg_Dbg(opts.logger, "Spawned child PID %d", static_cast<int>(pid));

    return true;
}

std::string ProcessRunner::readAllStdout()
{
    /* Read all data from the stdout pipe using standard read().
     * The pipe was set to non-blocking, so we poll + sleep to allow
     * cancellation checks. */
    while (true)
    {
        ssize_t n = readFd(m_stdoutFd, m_stdoutBuf, 4096);
        if (n > 0)
            continue; /* More data available */
        if (n == 0)
            break; /* EOF */
        /* n < 0: no data right now (EAGAIN/EWOULDBLOCK) or error.
         * If EAGAIN, the process is still running — we'll come back. */
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            break; /* Real error */
        /* No data yet — yield briefly and retry */
        struct timespec ts = { 0, 10000000 }; /* 10ms */
        nanosleep(&ts, nullptr);
    }
    return m_stdoutBuf;
}

std::string ProcessRunner::readStderrLine(CancellationToken* token)
{
    /* Try to find a complete line in the buffer first */
    while (true)
    {
        size_t pos = m_stderrBuf.find('\n');
        if (pos != std::string::npos)
        {
            std::string line = m_stderrBuf.substr(0, pos);
            /* Remove trailing \r if present (Windows line endings) */
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            m_stderrBuf.erase(0, pos + 1);
            return line;
        }

        /* Read more data */
        char buf[4096];
        ssize_t n = read(m_stderrFd, buf, sizeof(buf));
        if (n <= 0)
        {
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            {
                /* EOF or real error: return whatever is left */
                if (!m_stderrBuf.empty())
                {
                    std::string last = std::move(m_stderrBuf);
                    m_stderrBuf.clear();
                    if (!last.empty() && last.back() == '\r')
                        last.pop_back();
                    return last;
                }
                return {};
            }
            /* EAGAIN — no data available right now.
             * Check cancellation token to allow aborting when the
             * user cancels the download — otherwise we'd be stuck
             * in this loop until the next progress line arrives from
             * the process. */
            if (token && token->isCancelled())
            {
                /* Return empty to signal cancellation to caller.
                 * The caller (YtdlpStrategy::execute) will detect
                 * the cancellation state and kill the process. */
                return {};
            }
            /* EAGAIN — no data yet, sleep and retry */
            struct timespec ts = { 0, 10000000 }; /* 10ms */
            nanosleep(&ts, nullptr);
            continue;
        }
        m_stderrBuf.append(buf, static_cast<size_t>(n));
    }
}

int ProcessRunner::wait()
{
    if (!m_started || m_pid < 0)
        return -1;

    m_running = false;

    int status;
    pid_t result;
    do {
        result = waitpid(m_pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    m_pid = -1;

    if (result == -1)
        return -1; /* waitpid failed */

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return -WTERMSIG(status); /* Negative = killed by signal */
    return -1;
}

void ProcessRunner::cancel()
{
    if (!m_running || m_pid < 0)
        return;

    pid_t pid = m_pid;
    m_running = false;

    /* Send SIGTERM first — gives the process a chance to clean up */
    kill(pid, SIGTERM);

    /* Wait briefly for the process to exit cleanly.
     * Using a short timeout avoids blocking the UI thread while still
     * ensuring the process is actually dead before we return.
     * If the process doesn't exit within the timeout, send SIGKILL. */
    const int maxWaitMs = 3000; /* 3 seconds */
    const int pollIntervalMs = 50;
    int waited = 0;

    while (waited < maxWaitMs)
    {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid)
        {
            /* Process exited — we're done */
            m_pid = -1;
            return;
        }
        if (result == -1 && errno != EINTR)
        {
            /* waitpid failed (e.g., ECHILD — already reaped) — stop trying */
            break;
        }

        struct timespec ts = { 0, pollIntervalMs * 1000000 };
        nanosleep(&ts, nullptr);
        waited += pollIntervalMs;
    }

    /* Timeout — process didn't exit cleanly. Force kill. */
    kill(pid, SIGKILL);

    /* Wait for the forced kill to complete */
    int status;
    waitpid(pid, &status, 0);
    m_pid = -1;
}

/* static */
ssize_t ProcessRunner::readFd(int fd, std::string& buffer, size_t chunkSize)
{
    if (fd < 0)
        return -1;

    /* Find the current end of the buffer and resize */
    size_t oldSize = buffer.size();
    buffer.resize(oldSize + chunkSize);

    ssize_t n = read(fd, &buffer[oldSize], chunkSize);
    if (n > 0)
    {
        buffer.resize(oldSize + static_cast<size_t>(n));
    }
    else
    {
        buffer.resize(oldSize);
    }
    return n;
}

} // namespace downloader
} // namespace vlc
