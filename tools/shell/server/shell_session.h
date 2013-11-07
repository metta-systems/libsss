#pragma once

#include "pid_watcher.h"

class shell_session : private shell_protocol
{
private:
    shell_stream shs;
    int ptyfd, ttyfd;
    // AsyncFile aftty;
    std::string termname;   // Name for TERM environment variable
    pid_watcher pidw;    // To watch for the child process's death

public:
    shell_session(ssu::stream *stream);
    ~shell_session();

private:
    void got_control(byte_array const& msg);

    void open_pty(SST::XdrStream &rxs);
    void run_shell(SST::XdrStream &rxs);
    void do_exec(SST::XdrStream &rxs);

    void run(std::string const& cmd = std::string());

    // Send an error message and reset the stream
    void error(std::string const& str);

// private slots:
    void in_ready();
    void out_ready();
    void child_done();
};
