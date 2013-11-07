#include "settings_provider.h"
#include "shell_server.h"
#include "host.h"

int main(int argc, char **argv)
{
    auto settings = settings_provider::instance();

    // Initialize SST and read or create our host identity
    auto host(ssu::host::create(settings.get()));

    // Create and register the shell service
    shell_server svc(host);

    host->run_io_service();
}
