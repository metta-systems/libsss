#include "shell_server.h"
#include "shell_session.h"

shell_server::shell_server(std::shared_ptr<ssu::host> host)
    : srv(host)
{
    srv.on_new_connection.connect([this]{got_connection();});

    if (!srv.listen(service_name, "Secure Remote Shell",
        protocol_name, "MettaNode Remote Shell Protocol"))
        logger::fatal() << "Can't register Shell service";
}

void shell_server::got_connection()
{
    while (auto stream = srv.accept())
    {
        new shell_session(stream);
    }
}
