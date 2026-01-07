#include "libsdb/bit.hpp"
#include "libsdb/pipe.hpp"
#include "libsdb/register_info.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <fstream>
#include <libsdb/error.hpp>
#include <libsdb/process.hpp>
#include <signal.h>
#include <string>

using namespace sdb;

const std::string test_dir = "./builds/build/test/";

namespace
{
char get_process_status(pid_t pid)
{
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    std::string data;

    std::getline(stat, data);
    auto index_of_last_parenthesis = data.rfind(')');
    auto index_of_status_indicator = index_of_last_parenthesis + 2;

    return data[index_of_status_indicator];
}

bool process_exists(pid_t pid)
{
    auto ret = kill(pid, 0);
    return ret != -1 and errno != ESRCH;
}
}; // namespace

TEST_CASE("process launch success", "[process]")
{
    auto process = process::launch("yes");
    REQUIRE(process_exists(process->pid()));
}

TEST_CASE("process::launch no such program", "[process]")
{
    REQUIRE_THROWS_AS(process::launch("you_do_not_have_to_be_good"), error);
}
#if defined(__linux__)
TEST_CASE("process::attach success", "[process]")
{
    auto target = process::launch(test_dir + "targets/run_endlessly", false);
    auto proc = process::attach(target->pid());
    REQUIRE(get_process_status(target->pid()) == 't');
}

TEST_CASE("process::resume success", "[process]")
{
    {
        auto proc = process::launch(test_dir + "targets/run_endlessly");

        proc->resume();

        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';

        REQUIRE(success);
    }

    {
        auto target = process::launch(test_dir + "targets/run_endlessly", false);
        auto proc = process::attach(target->pid());
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
}
#endif

TEST_CASE("process::resume already terminated", "[process]")
{
    auto proc = process::launch(test_dir + "targets/end_immediately");
    proc->resume();
    proc->wait_on_signal();
    REQUIRE_THROWS_AS(proc->resume(), error);
}

TEST_CASE("Write register works", "[register]")
{
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);
    auto proc = process::launch(test_dir + "targets/reg_write", true, channel.get_write());

    channel.close_write();

    // Check contents of rsi.

    proc->resume();
    proc->wait_on_signal();

    // Trap occurs.
    auto &regs = proc->get_registers();
    regs.write_by_id(register_id::rsi, 0xcafecafe);

    // Resume from trap.
    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();

    REQUIRE(to_string_view(output) == "0xcafecafe");

    // Check contents of mm0.
    regs.write_by_id(register_id::mm0, 0xcafecafe);

    // Resume from trap.
    proc->resume();
    proc->wait_on_signal();

    output = channel.read();

    REQUIRE(to_string_view(output) == "0xcafecafe");

    // Check contents of xmm0 (sse).
    regs.write_by_id(register_id::xmm0, 42.42);

    // Resume from trap.
    proc->resume();
    proc->wait_on_signal();

    output = channel.read();

    REQUIRE(to_string_view(output) == "42.42");
}
