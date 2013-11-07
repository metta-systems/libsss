#pragma once

#include "server.h"
#include "shell_protocol.h"

class shell_server : public shell_protocol
{
private:
    ssu::server srv;

    void got_connection();

public:
    shell_server(ssu::host *host);
};
