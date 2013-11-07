#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <QDir>
#include <QSettings>
#include <QCoreApplication>

#include "shell_server.h"

int main(int argc, char **argv)
{
    settings_provider settings;

    // Initialize SST and read or create our host identity
    ssu::host host(settings);

    // Create and register the shell service
    shell_server svc(&host);

    return host->run_io_service();
}
