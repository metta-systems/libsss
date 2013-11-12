#pragma once

#include "pid_watcher.h"
#include "async_file.h"

class shell_session : private shell_protocol
{
private:
    shell_stream shs;
    int ptyfd, ttyfd;
    async_file aftty;
    std::string termname;   // Name for TERM environment variable
    pid_watcher pidw;    // To watch for the child process's death

public:
    shell_session(std::shared_ptr<ssu::stream> stream);
    ~shell_session();

private:
    void got_control(byte_array const& msg);

    void open_pty(byte_array_iwrap<flurry::iarchive>& rxs);
    void run_shell(byte_array_iwrap<flurry::iarchive>& rxs);
    void do_exec(byte_array_iwrap<flurry::iarchive>& rxs);

    void run(std::string const& cmd = std::string());

    // Send an error message and reset the stream
    void error(std::string const& str);

    // Handlers:
    void in_ready();
    void out_ready();
    void child_done();
};
