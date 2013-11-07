#pragma once

#include <QThread>

class pid_watcher : public QThread
{
private:
    int pid;
    int stat;

public:
    pid_watcher();

    /// Start the PidWatcher thread and make it wait on process 'pid'.
    /// The thread terminates when the designated child process does,
    /// emitting the QThread::finished() signal.
    void watch_pid(int pid);

    /// After the PidWatcher thread has terminated and emitted finished(),
    /// the child process's exit status can be obtained from this method.
    inline int exit_status() { return stat; }

private:
    void run();
};
