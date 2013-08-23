#include "simulation/sim_link.h"
#include "simulation/sim_host.h"
#include "simulation/sim_packet.h"

namespace ssu {
namespace simulation {

sim_link::sim_link(std::shared_ptr<sim_host> host)
    : link(*host)
    , host_(host)
    , simulator_(host->get_simulator())
{}

sim_link::~sim_link()
{
    // unbind();
}

// Target address must be routable to in order to send.
// Find the destination host in the "routing table".
bool sim_link::send(const endpoint& ep, const char *data, size_t size)
{
    assert(port_ > 0);

    endpoint src;
    src.port(port_);
    std::shared_ptr<sim_host> dest_host = host_->neighbor_at(ep, src);
    if (!dest_host) {
        logger::warning() << "Unknown or non-adjacent target host " << ep;
        return false;
    }

    std::shared_ptr<sim_connection> pipe(host_->connection_at(src));
    assert(pipe);

    new sim_packet(host_, src, pipe, ep, byte_array(data, size)); //@todo keep reference around
    return true;
}

std::vector<endpoint>
sim_link::local_endpoints()
{
    return host_->local_endpoints();
}

} // simulation namespace
} // ssu namespace
