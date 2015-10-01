#include "arsenal/logging.h"
#include "comm/packet_receiver.h"
#include "uia/comm/socket_host_interface.h"

using namespace std;

namespace uia {
namespace comm {

//=================================================================================================
// packet_receiver
//=================================================================================================

void
packet_receiver::bind(string magic)
{
    assert(!is_bound());
    assert(magic.size() == 8);
    assert(!host_interface_->has_receiver_for(magic));

    magic_ = magic;
    logger::debug() << "Link receiver " << this << " binds for magic " << byte_array(magic_);
    host_interface_->bind_receiver(magic_, shared_from_this());
}

void
packet_receiver::unbind()
{
    if (is_bound())
    {
        logger::debug() << "Link receiver " << this << " unbinds magic " << byte_array(magic_);
        host_interface_->unbind_receiver(magic_);
        magic_.clear();
    }
}

} // comm namespace
} // uia namespace
