PidWatcher::PidWatcher(QObject *parent)
:   QThread(parent),
    pid(-1)
{
}

void PidWatcher::watchPid(int pid)
{
    Q_ASSERT(!isRunning() && !isFinished());

    this->pid = pid;
    start();
}

void PidWatcher::run()
{
    Q_ASSERT(pid > 0);
    waitpid(pid, &stat, 0);
}

