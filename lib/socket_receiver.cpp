#include "arsenal/logging.h"
#include "ssu/socket_receiver.h"
#include "ssu/host.h"

namespace ssu {

//=================================================================================================
// socket_receiver
//=================================================================================================

void socket_receiver::bind(magic_t magic)
{
    assert(!is_bound());
    // Receiver's magic value must leave the upper byte 0
    // to distinguish control packets from channel data packets.
    assert(magic <= 0xffffff);
    assert(!host_->has_receiver_for(magic));

    magic_ = magic;
    logger::debug() << "Link receiver " << this << " binds for magic " << hex(magic_, 8, true);
    host_->bind_receiver(magic_, this);
}

void socket_receiver::unbind()
{
    if (is_bound())
    {
        logger::debug() << "Link receiver " << this << " unbinds magic " << hex(magic_, 8, true);
        host_->unbind_receiver(magic_);
        magic_ = 0;
    }
}

} // ssu namespace
