#include "libsdb/error.hpp"
#include "libsdb/pipe.hpp"
#include "libsdb/register_info.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <libsdb/process.hpp>
#include <memory>
#include <optional>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
void exit_with_errno(sdb::pipe &channel, std::string const &prefix)
{
    auto message = prefix + ": " + std::strerror(errno);

    channel.write(reinterpret_cast<std::byte *>(message.data()), message.size());
    exit(-1);
}
} // namespace

std::unique_ptr<sdb::process> sdb::process::launch(std::filesystem::path path, bool debug,
                                                   std::optional<int> stdout_replacement)
{
    pid_t pid;
    pipe channel(true);

    if ((pid = fork()) < 0)
    {
        error::send_errno("fork failed");
    }

    if (pid == 0)
    {
        channel.close_read();
        if (stdout_replacement)
        {
            // This closes stdout file descriptor and duplicates the first one
            // to the second. This means that anything that goes to stdout now
            // goes to stdout_replacement.
            if (dup2(*stdout_replacement, STDOUT_FILENO) < 0)
                exit_with_errno(channel, "stdout replacement failed");
        }
        // Inside child process.
#if defined(__APPLE__) && defined(__MACH__)
        if (debug and ptrace(PT_TRACE_ME, 0, nullptr, 0) < 0)
#elif defined(__linux__)
        if (debug and ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0)
#endif
        {
            exit_with_errno(channel, "Tracing failed");
        }
        if (execlp(path.c_str(), path.c_str(), nullptr) < 0)
        {
            exit_with_errno(channel, "execlp failed");
        }
    }

    channel.close_write();
    auto data = channel.read();
    channel.close_read();

    if (data.size() > 0)
    {
        // wait for the child process to terminate and issue an error.
        waitpid(pid, nullptr, 0);
        auto chars = reinterpret_cast<char *>(data.data());
        error::send(std::string(chars, chars + data.size()));
    }

    std::unique_ptr<sdb::process> proc(new process(pid, true, debug));

    if (debug)
        proc->wait_on_signal();

    return proc;
}

std::unique_ptr<sdb::process> sdb::process::attach(pid_t pid)
{
    if (pid == 0)
    {
        error::send("Invalid PID");
    }

#if defined(__APPLE__) && defined(__MACH__)
    if (ptrace(PT_ATTACH, pid, nullptr, 0) < 0)
#elif defined(__linux__)
    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
#endif
    {
        error::send_errno("Could not attach");
    }

    std::unique_ptr<sdb::process> proc(new process(pid, false, true));
    proc->wait_on_signal();

    return proc;
}

void sdb::process::resume()
{
#if defined(__APPLE__) && defined(__MACH__)
    // In macos, addr, for PT_CONTINUE, should not be nullptr. Using
    // reinterpret_cast<caddr_t>(1) as addr, indicates the current process
    // should be resumed.
    if (ptrace(PT_CONTINUE, pid_, reinterpret_cast<caddr_t>(1), 0) < 0)
#elif defined(__linux__)
    if (ptrace(PTRACE_CONT, pid_, nullptr, nullptr) < 0)
#endif
    {
        error::send_errno("Could not resume");
    }
    state_ = process_state::running;
}

sdb::stop_reason sdb::process::wait_on_signal()
{
    int wait_status;
    int options = 0;

    if (waitpid(pid_, &wait_status, options) < 0)
    {
        error::send_errno("waitpid failed");
    }

    stop_reason reason(wait_status);
    state_ = reason.reason;

    if (is_attached_ and state_ == process_state::stopped)
        read_all_registers();

    return reason;
}

sdb::process::~process()
{
    if (pid_ != 0)
    {
        int status;

        if (is_attached_)
        {
            // In order for PTRACE_DETACH to work, the inferior process must've
            // been stopped.
            if (state_ == process_state::running)
            {
                kill(pid_, SIGSTOP);
                waitpid(pid_, &status, 0);
            }

#if defined(__APPLE__) && defined(__MACH__)
            ptrace(PT_DETACH, pid_, nullptr, 0);
#elif defined(__linux__)
            ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
#endif
            // Once de-attached, let the process continue running.
            kill(pid_, SIGCONT);
        }
        if (terminate_on_end_)
        {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

sdb::stop_reason::stop_reason(int wait_status)
{
    if (WIFEXITED(wait_status))
    {
        reason = process_state::exited;
        info = WEXITSTATUS(wait_status);
    }
    else if (WIFSIGNALED(wait_status))
    {
        reason = process_state::terminated;
        info = WTERMSIG(wait_status);
    }
    else if (WIFSTOPPED(wait_status))
    {
        reason = process_state::stopped;
        info = WSTOPSIG(wait_status);
    }
}

void sdb::process::read_all_registers()
{
    if (ptrace(PTRACE_GETREGS, pid_, nullptr, &get_registers().data_.regs) < 0)
        error::send_errno("Could not read GPR registers");
    if (ptrace(PTRACE_GETFPREGS, pid_, nullptr, &get_registers().data_.i387) < 0)
        error::send_errno("Could not read FPR registers");

    for (int i = 0; i < 8; ++i)
    {
        auto id = static_cast<int>(register_id::dr0) + i;
        auto info = register_info_by_id(static_cast<register_id>(id));

        errno = 0;

        std::uint64_t data = ptrace(PTRACE_PEEKUSER, pid_, info.offset, nullptr);
        if (errno != 0)
            error::send_errno("Could not read debug register");

        get_registers().data_.u_debugreg[i] = data;
    }
}

/// Used to write a single GPR or Debug Regiter value.
void sdb::process::write_user_area(std::size_t offset, std::uint64_t data)
{
    if (ptrace(PTRACE_POKEUSER, pid_, offset, data) < 0)
        error::send_errno("Could not write to user area");
}

void sdb::process::write_gprs(const user_regs_struct &gprs)
{
    if (ptrace(PTRACE_SETREGS, pid_, nullptr, &gprs) < 0)
        error::send_errno("Could not write general purpose registers");
}

void sdb::process::write_fprs(const user_fpregs_struct &fprs)
{
    if (ptrace(PTRACE_SETFPREGS, pid_, nullptr, &fprs) < 0)
        error::send_errno("Could not write general purpose registers");
}
