#include "base_stream.h"

namespace ssu {

base_stream::base_stream(std::shared_ptr<host>& h, const peer_id& peer, std::shared_ptr<base_stream> parent)
    : abstract_stream(h)
{}

base_stream::~base_stream()
{}

}
