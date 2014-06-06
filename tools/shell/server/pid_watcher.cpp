//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <sys/types.h>
#include <sys/wait.h>
#include "pid_watcher.h"

void pid_watcher::watch_pid(int pid)
{
    thread_ = std::thread([this, pid]{
        waitpid(pid, &stat_, 0);
        on_finished();
    });
}
