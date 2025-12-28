#include <cstddef>
#include <fcntl.h>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <unistd.h>
#include <utility>
#include <vector>

sdb::pipe::pipe(bool close_on_exec)
{
#if defined(__APPLE__) && defined(__MACH__)
    // macos does not have any pipe2 defined, so we need to
    if (::pipe(fds_) < 0)
        sdb::error::send_errno("Pipe creation failed");
    if (close_on_exec)
    {
        if (fcntl(fds_[0], F_SETFD, FD_CLOEXEC) < 0)
            sdb::error::send_errno("Could not set FD_CLOEXEC for read");
        if (fcntl(fds_[1], F_SETFD, FD_CLOEXEC) < 0)
            sdb::error::send_errno("Could not set FD_CLOEXEC for write");
    }
#elif defined(__linux__)
    if (pipe2(fds_, close_on_exec ? O_CLOEXEC : 0) < 0)
        sdb::error::send_errno("Pipe creation failed");
#endif
}

sdb::pipe::~pipe()
{
    close_read();
    close_write();
}

std::vector<std::byte> sdb::pipe::read()
{
    char buf[1024];
    int chars_read;

    if ((chars_read = ::read(fds_[read_fd], buf, sizeof(buf))) < 0)
    {
        error::send_errno("Could not read from pipe");
    }

    auto bytes = reinterpret_cast<std::byte *>(buf);

    return std::vector<std::byte>(bytes, bytes + chars_read);
}

void sdb::pipe::write(std::byte *from, std::size_t bytes)
{
    if ((::write(fds_[write_fd], from, bytes)) < 0)
    {
        error::send_errno("Could not write to pipe");
    }
}

int sdb::pipe::release_read()
{
    return std::exchange(fds_[read_fd], -1);
}

int sdb::pipe::release_write()
{
    return std::exchange(fds_[write_fd], -1);
}

void sdb::pipe::close_read()
{
    if (fds_[read_fd] != -1)
    {
        ::close(fds_[read_fd]);
        fds_[read_fd] = -1;
    }
}

void sdb::pipe::close_write()
{
    if (fds_[write_fd] != -1)
    {
        ::close(fds_[write_fd]);
        fds_[write_fd] = -1;
    }
}
