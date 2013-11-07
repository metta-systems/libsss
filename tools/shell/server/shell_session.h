#pragma once

#include <QThread>

#include "stream.h"
#include "proto.h"
#include "asyncfile.h"

class shell_session : private shell_protocol
{
private:
    shell_stream shs;
    int ptyfd, ttyfd;
    AsyncFile aftty;
    QString termname;   // Name for TERM environment variable 
    PidWatcher pidw;    // To watch for the child process's death

public:
    ShellSession(SST::Stream *strm, QObject *parent = NULL);
    ~ShellSession();

private:
    void gotControl(const QByteArray &msg);

    void openPty(SST::XdrStream &rxs);
    void runShell(SST::XdrStream &rxs);
    void doExec(SST::XdrStream &rxs);

    void run(const QString &cmd = QString());

    // Send an error message and reset the stream
    void error(const QString &str);

private slots:
    void inReady();
    void outReady();
    void childDone();
};
