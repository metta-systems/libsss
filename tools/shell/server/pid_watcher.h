//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <thread>
#include <boost/signals2/signal.hpp>

class pid_watcher
{
private:
    int stat_{0};
    std::thread thread_;

public:
    pid_watcher() = default;

    /**
     * Start the pid watcher thread and make it wait on process 'pid'.
     * The thread terminates when the designated child process does,
     * emitting the on_finished() signal.
     */
    void watch_pid(int pid);

    /**
     * After the pid watcher thread has terminated and emitted on_finished(),
     * the child process's exit status can be obtained from this method.
     */
    inline int exit_status() { return stat_; }

    boost::signals2::signal<void (void)> on_finished;
};
