#ifndef SDB_PROCESS_HPP
#define SDB_PROCESS_HPP

#include <chrono>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sys/types.h>

namespace sdb
{
enum class process_state
{
    stopped,
    running,
    exited,
    terminated
};

class process
{
    process() = delete;
    process(const process &) = delete;
    process &operator=(const process &) = delete;

  public:
    static std::unique_ptr<process> launch(std::filesystem::path path);
    static std::unique_ptr<process> attach(pid_t pid);

    ~process();

    void resume();
    // ? wait_on_signal();

    pid_t pid() const
    {
        return pid_;
    }

    process_state state() const
    {
        return state_;
    }

  private:
    pid_t pid_ = 0;
    bool terminate_on_end_ = true;
    process_state state_ = process_state::stopped;

    process(pid_t pid, bool terminate_on_end) : pid_(pid), terminate_on_end_(terminate_on_end)
    {
    }
};
} // namespace sdb

#endif
