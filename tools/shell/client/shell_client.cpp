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
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <boost/log/trivial.hpp>
#include "shell_client.h"
#include "arsenal/byte_array_wrap.h"

using namespace sss;
using namespace std;

#define STDIN_FILENO 0

static bool termiosChanged{false};
static struct termios termiosSave;

static void termiosRestore()
{
    if (!termiosChanged)
        return;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosSave) < 0)
        BOOST_LOG_TRIVIAL(debug) << "Can't restore terminal settings: " << strerror(errno);

    termiosChanged = false;
}

shell_client::shell_client(shared_ptr<sss::host> host)
    : stream_(make_shared<sss::stream>(host))
    , shs(stream_)
    , afin(host->get_io_service())
    , afout(host->get_io_service())
{
    afin.on_ready_read.connect([this] { in_ready(); });
    stream_->on_bytes_written.connect([this](ssize_t) { in_ready(); });

    stream_->on_ready_read.connect([this] { out_ready(); });
    afout.on_bytes_written.connect([this](ssize_t) { out_ready(); });
}

void shell_client::setup_terminal(int fd)
{
    BOOST_LOG_TRIVIAL(debug) << "Shell client setup terminal on fd " << fd;

    // Get current terminal name
    string termname(getenv("TERM"));

    // Get current terminal settings
    struct termios tios;
    if (tcgetattr(fd, &tios) < 0)
        BOOST_LOG_TRIVIAL(fatal) << "Can't get terminal settings: " << strerror(errno);

    // Save the original terminal settings,
    // and install an atexit handler to restore them on exit.
    if (!termiosChanged) {
        assert(fd == STDIN_FILENO);   // XX
        termiosSave = tios;
        termiosChanged = true;
        atexit(termiosRestore);
    }

    // Get current window size
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(fd, TIOCGWINSZ, &ws) < 0)
        BOOST_LOG_TRIVIAL(warning) << "Can't get terminal window size: " << strerror(errno);

    // Build the pseudo-tty parameter control message
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << Terminal << termname
            << ws.ws_col << ws.ws_row
            << ws.ws_xpixel << ws.ws_ypixel;
        termpack(write.archive(), tios);
    }

    // Send it
    shs.send_control(msg);

    // Turn off terminal input processing
    tios.c_lflag &= ~(ICANON | ISIG | ECHO);
    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0) {
        BOOST_LOG_TRIVIAL(fatal) << "Can't set terminal settings: " << strerror(errno);
    }
}

void shell_client::run_shell(string const& cmd, int infd, int outfd)
{
    if (!afin.open(infd, afin.Read)) {
        BOOST_LOG_TRIVIAL(fatal) << "Error setting up input forwarding: " << afin.error_string();
    }

    if (!afout.open(outfd, afout.Write)) {
        BOOST_LOG_TRIVIAL(fatal) << "Error setting up output forwarding: " << afout.error_string();
    }

    // Build the message to start the shell or command
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        if (cmd.empty())
            write.archive() << Shell;
        else
            write.archive() << Exec << cmd;
    }
    shs.send_control(msg);
}

void shell_client::in_ready()
{
    BOOST_LOG_TRIVIAL(debug) << "Shell client in ready";
    while (true) {
        // XX if (shs.bytesToWrite() >= shellBufferSize) return;

        char buf[4096];
        int act = afin.read(buf, sizeof(buf));
        //BOOST_LOG_TRIVIAL(debug) << this << "got:" << byte_array(buf, act);
        if (act < 0) {
            BOOST_LOG_TRIVIAL(fatal) << "Error reading input for remote shell: " << afin.error_string();
        }

        if (act == 0) {
            if (afin.at_end()) {
                BOOST_LOG_TRIVIAL(debug) << "End of local input";
                afin.close_read();
                shs.stream()->shutdown(stream::shutdown_mode::write);
            }
            return;
        }
        shs.send_data(buf, act);
    }
}

void shell_client::out_ready()
{
    BOOST_LOG_TRIVIAL(debug) << "Shell client out ready";
    while (true) {
        if (afout.bytes_to_write() >= shellBufferSize)
            return; // Wait until the write buffer empties a bit

        shell_stream::packet pkt = shs.receive();
        switch (pkt.type) {
        case shell_stream::packet_type::Null:
            if (shs.at_end()) {
                BOOST_LOG_TRIVIAL(debug) << "End of remote shell stream";
                exit(0);
            }
            return; // Nothing more to receive for now
        case shell_stream::packet_type::Data:
            if (afout.write(pkt.data) < 0) {
                BOOST_LOG_TRIVIAL(fatal) << "Error writing remote shell output: " << afout.error_string();
            }
            break;
        case shell_stream::packet_type::Control:
            got_control_packet(pkt.data);
            break;
        }
    }
}

void shell_client::got_control_packet(byte_array const& msg)
{
    BOOST_LOG_TRIVIAL(debug) << "Shell client got control message, size " << dec << msg.size();

    byte_array_iwrap<flurry::iarchive> read(msg);
    int32_t cmd;
    read.archive() >> cmd;
    switch (cmd) {
    case ExitStatus:
    {
        int32_t code;
        read.archive() >> code;
        // if (rxs.status() != rxs.Ok)
            // BOOST_LOG_TRIVIAL(debug) << "invalid ExitStatus control message";
        BOOST_LOG_TRIVIAL(debug) << "remote process exited with code " << code;
        exit(code);
        break;
    }

    case ExitSignal:
    {
        int32_t flags;
        string signame, errmsg, langtag;
        read.archive() >> flags >> signame >> errmsg >> langtag;
        // if (rxs.status() != rxs.Ok)
            // BOOST_LOG_TRIVIAL(debug) << "invalid ExitSignal control message";

        BOOST_LOG_TRIVIAL(info) << "Remote process terminated by signal " << signame
            << ((flags & 1) ? " (core dumped)" : "");

        exit(1);
        break;
    }

    default:
        BOOST_LOG_TRIVIAL(debug) << "unknown control message type " << cmd;
        break;      // just ignore the control message
    }
}

