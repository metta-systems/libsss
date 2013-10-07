#include "host.h"

namespace ssu {

host::host(settings_provider* settings, uint16_t default_port)
{
    init_link(settings, default_port);
}

} // ssu namespace
