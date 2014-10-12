#include "arsenal/logging.h"
#include "comm/socket_receiver.h"

using namespace std;

namespace uia {
namespace comm {

//=================================================================================================
// socket_receiver
//=================================================================================================

void
socket_receiver::bind(string magic)
{
    assert(!is_bound());
    assert(magic.size() == 8);
    assert(!host_interface_->has_receiver_for(magic));

    magic_ = magic;
    logger::debug() << "Link receiver " << this << " binds for magic " << magic_;
    host_interface_->bind_receiver(magic_, shared_from_this());
}

void
socket_receiver::unbind()
{
    if (is_bound())
    {
        logger::debug() << "Link receiver " << this << " unbinds magic " << magic_;
        host_interface_->unbind_receiver(magic_);
        magic_.clear();
    }
}

} // comm namespace
} // uia namespace
