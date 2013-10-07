#include "host.h"

using namespace std;

namespace ssu {

shared_ptr<host>
host::create()
{
    shared_ptr<host> host(make_shared<host>());
    return host;
}

shared_ptr<host>
host::create(settings_provider* settings, uint16_t default_port)
{
    shared_ptr<host> host(make_shared<host>());
    host->init_link(settings, default_port);
    return host;
}

} // ssu namespace
