//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include "shell_session.h"
#include "arsenal/logging.h"
#include "sss/host.h"

using namespace std;

shell_session::shell_session(std::shared_ptr<sss::stream> stream)
    : shs(stream)
    , ptyfd(-1)
    , ttyfd(-1)
    , aftty(stream->get_host()->get_io_service())
{
    logger::debug() << "shell_session";

    shs.on_ready_read.connect([this]{in_ready();});
    aftty.on_bytes_written.connect([this](ssize_t){in_ready();});

    aftty.on_ready_read.connect([this]{out_ready();});
    shs.on_bytes_written.connect([this](ssize_t){out_ready();});

    pidw.on_finished.connect([this]{child_done();});
}

shell_session::~shell_session()
{
    logger::debug() << "~shell_session";

    aftty.close();

    if (ptyfd >= 0)
        close(ptyfd);
    if (ttyfd >= 0)
        close(ttyfd);
}

void shell_session::in_ready()
{
    bool ttyopen = aftty.is_open();
    while (true) {
        if (ttyopen and aftty.bytes_to_write() >= shellBufferSize)
            return; // wait until the write buffer empties a bit

        shell_stream::packet pkt = shs.receive();
        switch (pkt.type) {
            case shell_stream::packet_type::Null:
                if (shs.at_end()) {
                    logger::debug() << "End of remote input";
                    // aftty.close_write();
                }
                return; // nothing more to receive for now

            case shell_stream::packet_type::Data:
                logger::debug() << this << "input:" << pkt.data;
                if (!ttyopen) {
                    error("Received shell data before command to start shell");
                    break;
                }
                if (aftty.write(pkt.data) < 0)
                    error(aftty.error_string());
                break;

            case shell_stream::packet_type::Control:
                got_control_packet(pkt.data);
                break;
        }
    }
}

void shell_session::out_ready()
{
    assert(aftty.is_open());

    while (true) {
        // XX if (shs.bytesToWrite() >= shellBufferSize) return;

        char buf[4096];
        int act = aftty.read(buf, sizeof(buf));
        if (act <= 0) {
            if (act < 0)
                error(aftty.error_string());
            return;
        }
        logger::debug() << "output: " << byte_array(buf, act);
        shs.send_data(buf, act);

        // When our child process(es) have no more output to send,
        // just close our end of the pipe and stop forwarding data
        // until the child process dies and we get its exit status.
        // (XX notify client via control message?)
        if (aftty.at_end()) {
            logger::debug() << "End-of-file on child pseudo-tty";
            aftty.close();
        }
    }
}

void shell_session::got_control_packet(byte_array const& msg)
{
    assert(msg.size() > 0);

    byte_array_iwrap<flurry::iarchive> read(msg);
    int32_t cmd;
    read.archive() >> cmd;
    switch ((Command)cmd) {
        case Terminal:  open_pty(read);   break;
        case Shell: run_shell(read);  break;
        case Exec:  do_exec(read);    break;
        default:
            logger::debug() << "Ignoring unknown control message type " << cmd;
    }
}

void shell_session::open_pty(byte_array_iwrap<flurry::iarchive>& rxs)
{
    if (ptyfd >= 0) {
        return error("Already have a pseudo-terminal");
    }
    if (ttyfd >= 0) {
        return error("Already have a remote shell I/O stream");
    }

    // Decode the rest of the Terminal control message
    uint32_t width, height, xpixels, ypixels;
    struct termios tios;
    rxs.archive() >> termname >> width >> height >> xpixels >> ypixels;
    termunpack(rxs.archive(), tios);
    // if (rxs.status() != rxs.Ok)
        // return error("Invalid Terminal request");

    logger::debug() << "Terminal " << termname << ", window " << width << "x" << height;

    ptyfd = posix_openpt(O_RDWR | O_NOCTTY);
    if (ptyfd < 0)
    {
        ostringstream oss;
        oss << "Can't create pseudo-terminal: " << strerror(errno);
        return error(oss.str());
    }

    // Set the pty's window size
    struct winsize ws;
    ws.ws_col = width;
    ws.ws_row = height;
    ws.ws_xpixel = xpixels;
    ws.ws_ypixel = ypixels;
    if (ioctl(ptyfd, TIOCSWINSZ, &ws) < 0) {
        logger::debug() << "Can't set terminal window size";
    }

    // Set the pty's terminal modes
    if (tcsetattr(ptyfd, TCSANOW, &tios) < 0) {
        logger::debug() << "Can't set terminal parameters";
    }
}

void shell_session::run_shell(byte_array_iwrap<flurry::iarchive>&)
{
    logger::debug() << "Run shell";
    run();
}

void shell_session::do_exec(byte_array_iwrap<flurry::iarchive>& rxs)
{
    string cmd;
    rxs.archive() >> cmd;
    // if (rxs.status() != rxs.Ok)
        // return error("Invalid Exec request");

    logger::debug() << "Run command " << cmd;

    run(cmd);
}

void shell_session::run(string const&/*cmd XXX*/)
{
    if (ttyfd >= 0) {
        return error("Already have a remote shell running");
    }

    // If we don't have a pseudo-terminal,
    // create a socket pair for the shell's (non-terminal) stdio.
    int childfd = -1;
    if (ptyfd < 0) {
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
            ostringstream oss;
            oss << "Can't create socket pair: " << strerror(errno);
            return error(oss.str());
        }
        ttyfd = fds[0];
        childfd = fds[1];
    }
    assert((ptyfd >= 0) ^ (ttyfd >= 0));  // one or the other, not both

    // Fork off the child process.
    pid_t childpid = fork();
    if (childpid < 0) {
        if (childfd >= 0)
            close(childfd);
        ostringstream oss;
        oss << "Can't create child process: " << strerror(errno);
        return error(oss.str());
    }
    if (childpid == 0) {
        // We're the child process.

        // XXX authenticate user, setuid appropriately

        if (ptyfd >= 0) {
            logger::debug() << "Setup child ptys";

            // Set up the pseudo-tty for the child
            signal(SIGCHLD, SIG_DFL);
            if (grantpt(ptyfd) < 0) {
                perror("Remote shell: grantpt");
                exit(1);
            }
            if (unlockpt(ptyfd) < 0) {
                perror("Remote shell: unlockpt");
                exit(1);
            }

            // Establish a new session
            if (setsid() < 0) {
                perror("Remote shell: setsid");
                exit(1);
            }

            // Open the child end of the terminal
            assert(childfd < 0);
            char *ttyname = ptsname(ptyfd);
            logger::debug() << "Child ptsname " << ttyname;
            if (ttyname == NULL) {
                perror("Remote shell: ptsname");
                exit(1);
            }
            childfd = open(ttyname, O_RDWR);
            if (childfd < 0) {
                perror("Remote shell: open tty");
                exit(1);
            }

            // Set the TERM environment variable appropriately
            setenv("TERM", termname.c_str(), 1);
        }
        assert(childfd >= 0);

        // Set up our stdio handles
        if (dup2(childfd, STDIN_FILENO) < 0 ||
                dup2(childfd, STDOUT_FILENO) < 0 ||
                dup2(childfd, STDERR_FILENO) < 0)
        {
            perror("Remote shell: dup2");
            exit(1);
        }

        // Run the shell/command
        logger::debug() << "child: exec";
        execlp("login", "login", NULL); // XXX
        perror("Remote shell: exec");
        exit(1);
    }

    // We're the parent process.
    // First close the child's file descriptor, if any.
    if (childfd >= 0) {
        close(childfd);
    }

    // Set up for I/O forwarding.
    aftty.open(ptyfd >= 0 ? ptyfd : ttyfd, aftty.ReadWrite);

    // Watch for the child process's termination.
    pidw.watch_pid(childpid);

    logger::debug() << "Started shell";
}

void shell_session::error(std::string const& str)
{
    logger::debug() << "Error: " << str;

    // XXX send error control message
    // this->deleteLater();
}

void shell_session::child_done()
{
    int rc = pidw.exit_status();
    logger::debug() << "Child terminated with status " << rc;

    byte_array cmsg;
    if (WIFEXITED(rc)) {
        // Regular process termination
        byte_array_owrap<flurry::oarchive> write(cmsg);
        write.archive() << ExitStatus << WEXITSTATUS(rc);

    } else if (WIFSIGNALED(rc)) {
        // Process terminated by signal
        string signame;
        switch (WTERMSIG(rc)) {
            case SIGABRT:   signame = "SIGABRT"; break;
            case SIGALRM:   signame = "SIGALRM"; break;
            case SIGBUS:    signame = "SIGBUS"; break;
            case SIGCHLD:   signame = "SIGCHLD"; break;
            case SIGCONT:   signame = "SIGCONT"; break;
            case SIGFPE:    signame = "SIGFPE"; break;
            case SIGHUP:    signame = "SIGHUP"; break;
            case SIGILL:    signame = "SIGILL"; break;
            case SIGINT:    signame = "SIGINT"; break;
            case SIGKILL:   signame = "SIGKILL"; break;
            case SIGPIPE:   signame = "SIGPIPE"; break;
            case SIGQUIT:   signame = "SIGQUIT"; break;
            case SIGSEGV:   signame = "SIGSEGV"; break;
            case SIGSTOP:   signame = "SIGSTOP"; break;
            case SIGTERM:   signame = "SIGTERM"; break;
            case SIGTSTP:   signame = "SIGTSTP"; break;
            case SIGTTIN:   signame = "SIGTTIN"; break;
            case SIGTTOU:   signame = "SIGTTOU"; break;
            case SIGUSR1:   signame = "SIGUSR1"; break;
            case SIGUSR2:   signame = "SIGUSR2"; break;
#ifdef SIGPOLL
            case SIGPOLL:   signame = "SIGPOLL"; break;
#endif
            case SIGPROF:   signame = "SIGPROF"; break;
            case SIGSYS:    signame = "SIGSYS"; break;
            case SIGTRAP:   signame = "SIGTRAP"; break;
            case SIGURG:    signame = "SIGURG"; break;
            case SIGVTALRM: signame = "SIGVTALRM"; break;
            case SIGXCPU:   signame = "SIGXCPU"; break;
            case SIGXFSZ:   signame = "SIGXFSZ"; break;
            default:
                signame = boost::lexical_cast<string>(WTERMSIG(rc));
                // XX append '@machine-type', like ssh?
                break;
        }
        int32_t flags = 0;
        if (WCOREDUMP(rc))
            flags |= 1;
        string errmsg;
        string langtag;        // XXX RFC3066 lang tag?

        byte_array_owrap<flurry::oarchive> write(cmsg);
        write.archive() << ExitSignal << flags << signame << errmsg << langtag;
    }
    if (!cmsg.is_empty()) {
        shs.send_control(cmsg);
    }

    // Terminate this shell session
    // this->deleteLater();
}
