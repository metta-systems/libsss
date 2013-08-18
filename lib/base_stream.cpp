#include "base_stream.h"

namespace ssu {

base_stream::base_stream(std::shared_ptr<host> h, 
                         const peer_id& peer,
                         std::shared_ptr<base_stream> parent)
    : abstract_stream(h)
{}

base_stream::~base_stream()
{}

//txenqflow()
void base_stream::tx_enqueue_channel(bool tx_immediately)
{
    if (!attached())
        return tx_attach();

    // stream_channel* channel = tx_current_attachment->channel;
    // assert(channel && channel->is_active());

    // if (!tx_enqueued_channel)
    // {
    //     if (tx_queue.empty())
    //     {
    //         if (owner) {
    //             owner->ready_write();
    //         }
    //     }
    //     else
    //     {
    //         channel->enqueue_stream(this);
    //         tx_enqueued_channel = true;
    //     }
    // }

    // if (tx_immediately && channel->may_transmit())
    //     channel->ready_transmit();
}

//calcReceiveWindow
void base_stream::recalculate_receive_window()
{}

//calc
void base_stream::recalculate_transmit_window()
{}

}
