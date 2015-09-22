//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "arsenal/logging.h"
#include "arsenal/flurry.h"
#include "arsenal/algorithm.h"
#include "arsenal/byte_array_wrap.h"
#include "sss/streams/base_stream.h"
#include "sss/streams/datagram_stream.h"
#include "sss/host.h"
#include "sss/channels/stream_channel.h"
#include "sss/server.h"
#include "sss/internal/stream_peer.h"

using namespace std;

namespace sss {

//=================================================================================================
// base_stream
//=================================================================================================

constexpr int base_stream::max_attachments;

base_stream::ptr
base_stream::create(host_ptr host, uia::peer_identity const& peer_id, base_stream_ptr parent)
{
    auto p = std::make_shared<base_stream>(host, peer_id, parent, private_tag{});
    // Insert us into the peer's master list of streams
    p->peer_->all_streams_.insert(p);
    return p;
}

base_stream::base_stream(host_ptr host,
                         uia::peer_identity const& peer_id,
                         base_stream_ptr parent,
                         private_tag)
    : abstract_stream(host)
    , parent_(parent)
{
    assert(!peer_id.is_null());

    logger::debug() << "Constructing base stream " << this << " for peer " << peer_id;

    // Initialize inherited parameters
    if (parent) {
        if (parent->listen_mode() bitand stream::listen_mode::inherit)
            listen(parent->listen_mode());
        set_receive_buffer_size(parent->child_receive_buf_size_);
        set_child_receive_buffer_size(parent->child_receive_buf_size_);
    }

    recalculate_receive_window();

    peer_id_ = peer_id;
    peer_    = host->stream_peer(peer_id);

    // Initialize the stream back-pointers in the attachment slots.
    for (int i = 0; i < max_attachments; ++i) {
        tx_attachments_[i].stream_ = this;
        rx_attachments_[i].stream_ = this;
    }
}

base_stream::~base_stream()
{
    logger::debug() << "Destructing base stream";
    clear();
}

void
base_stream::clear()
{
    state_    = state::disconnected;
    end_read_ = end_write_ = true;

    // De-register us from our peer
    if (peer_) {
        if (contains(peer_->usid_streams_, usid_)) {
            if (peer_->usid_streams_[usid_].lock() == shared_from_this()) {
                peer_->usid_streams_.erase(usid_);
            }
        }
        peer_->all_streams_.erase(shared_from_this());
        peer_ = nullptr;
    }

    // Clear any attachments we may have
    for (int i = 0; i < max_attachments; ++i) {
        tx_attachments_[i].clear();
        rx_attachments_[i].clear();
    }

    // Reset any unaccepted incoming substreams too
    for (auto sub : received_substreams_) {
        sub->shutdown(stream::shutdown_mode::reset);
        // should self-destruct automatically when done - clear() call below does it
    }
    received_substreams_.clear();
    received_datagrams_.clear();
}

bool
base_stream::is_attached()
{
    return tx_current_attachment_ != nullptr;
}

void
base_stream::transmit_on(stream_channel* channel)
{
    assert(tx_enqueued_channel_);
    assert(tx_current_attachment_ != nullptr);
    assert(channel == tx_current_attachment_->channel_);
    assert(!tx_queue_.empty());

    logger::debug() << "Base stream transmit_on channel " << channel;

    tx_enqueued_channel_ = false; // Channel has just dequeued us.

    // First garbage-collect any segments that have already been ACKed;
    // this can happen if we retransmit a segment but an ACK for the original arrives late.
    auto head_packet = &tx_queue_.front();

    while (head_packet->type() == frame_type::STREAM
           and !contains(tx_waiting_ack_, head_packet->tx_byte_seq_)) {
        // No longer waiting for this tsn - must have been ACKed.
        tx_queue_.pop_front();
        if (tx_queue_.empty()) {
            if (auto stream = owner_.lock()) {
                stream->on_ready_write();
            }
            return;
        }
        head_packet = &tx_queue_.front();
    }

    int seg_size = head_packet->payload_size();

    // Ensure our attachment has been acknowledged before using the SID.
    if (tx_current_attachment_->is_acknowledged()) {
        // Our attachment has been acknowledged, send the data packets freely.
        assert(!init_);
        assert(tx_current_attachment_->is_active());

        // Throttle data transmission if channel window is full
        if (tx_inflight_ + seg_size > tx_window_) {
            logger::debug() << "Transmit window full - need " << (tx_inflight_ + seg_size)
                            << " have " << tx_window_;
            // XXX query status if latest update is out-of-date!
            // XXXreturn;
        }

        // Datagrams get special handling.
        // @todo packet_type->frame_type
        // if (head_packet->type() == packet_type::datagram)
        //    return tx_datagram();

        // Register the segment as being in-flight.
        tx_inflight_ += seg_size;

        logger::debug() << "Inflight data " << head_packet->tx_byte_seq_ << ", bytes in flight "
                        << tx_inflight_;

        // Transmit the next segment in a regular Data packet.
        auto p = tx_queue_.front();
        tx_queue_.pop_front();

        // @todo packet_type->frame_type
        // assert(p.type() == packet_type::data);

        logger::debug() << p;

        // auto header       = p.header<data_header>();
        // header->stream_id = tx_current_attachment_->stream_id_;
        // Preserve flags already set.
        // header->type_subtype = type_and_subtype(packet_type::data, header->type_subtype);
        // header->window       = receive_window_byte();
        // header->tx_seq_no    = p.tx_byte_seq_; // Note: 32-bit TSN

        // Transmit
        return tx_data(p);
    }

    // See if we can potentially use an optimized attach/data packet;
    // this only works for regular stream segments, not datagrams,
    // and only within the first 2^16 bytes of the stream.
    // @todo packet_type->frame_type
    // if (head_packet->type() == packet_type::data and
    //    head_packet->tx_byte_seq_ <= 0xffff)
    {
        // See if we can attach stream using an optimized Init packet,
        // allowing us to indicate the parent with a short 16-bit LSID
        // and piggyback useful data onto the packet.
        // The parent must be attached to the same channel.
        // XXX probably should use some state invariant
        // in place of all these checks.
        if (top_level_) {
            parent_ = channel->root_;
        }

        shared_ptr<base_stream> parent = parent_.lock();

        if (init_ and parent and parent->tx_current_attachment_
            and parent->tx_current_attachment_->channel_ == channel
            and parent->tx_current_attachment_->is_active()
            and usid_.half_channel_id_ == channel->tx_channel_id()
            and uint16_t(usid_.counter_) == tx_current_attachment_->stream_id_
            /* XXX  and parent->tx_inflight_ + seg_size <= parent->tx_window_*/) {
            logger::debug() << "Sending optimized init packet with " << seg_size
                            << " payload bytes";

            // Adjust the in-flight byte count for channel control.
            // Init packets get "charged" to the parent stream.
            parent->tx_inflight_ += seg_size;
            logger::debug() << "Inflight init " << head_packet->tx_byte_seq_
                            << ", bytes in flight on parent " << parent->tx_inflight_;

            return tx_attach_data(frame_type::STREAM, parent->tx_current_attachment_->stream_id_);
        }

        // See if our peer has this stream in its SID space,
        // allowing us to attach using an optimized Reply packet.
        if (tx_inflight_ + seg_size <= tx_window_) {
            for (int i = 0; i < max_attachments; ++i) {
                if (rx_attachments_[i].channel_ == channel and rx_attachments_[i].is_active()) {
                    logger::debug() << "Sending optimized reply packet";

                    // Adjust the in-flight byte count.
                    tx_inflight_ += seg_size;
                    logger::debug() << "Inflight reply " << head_packet->tx_byte_seq_
                                    << ", bytes in flight " << tx_inflight_;

                    /// @todo khustup.
                    return tx_attach_data(frame_type::EMPTY, rx_attachments_[i].stream_id_);
                }
            }
        }
    }

    // We've exhausted all of our optimized-path options:
    // we have to send a specialized Attach packet instead of useful data.
    tx_attach();

    // Don't requeue onto our channel at this point -
    // we can't transmit any data until we get that ack!
}

void
base_stream::recalculate_receive_window()
{
    logger::debug() << "Base stream recalculate receive window";

    assert(receive_buf_size_ > 0);

    // Calculate the current receive window size
    size_t rwin = max(0, receive_buf_size_ - rx_buffer_used_);

    // If all of our buffer usage consists of out-of-order packets,
    // ensure that the sender can make progress toward filling the gaps.
    // This should only ever be an issue if we shrink the receive window abruptly,
    // leaving gaps in a formerly-large window.
    // (It's best just to avoid shrinking the receive window abruptly.)
    if (rx_available_ == 0 and rx_buffer_used_ > 0) {
        rwin = max(rwin, min_receive_buffer_size);
    }

    // Calculate the conservative receive window exponent
    int i = 0;
    while (size_t((2 << i) - 1) <= rwin) {
        i++;
    }
    receive_window_byte_ = i;

    logger::debug() << "Buffered " << dec << rx_available_ << "+"
                    << (rx_buffer_used_ - rx_available_) << ", new receive window " << rwin
                    << ", exp " << i;
}

void
base_stream::recalculate_transmit_window(uint8_t window_byte)
{
    int32_t old_window = tx_window_;

    if (window_byte > 158) // Spec 4.10.1
    {
        logger::warning() << "Received invalid window byte " << dec << window_byte;
        window_byte = 158;
    }

    // Calculate the new transmit window
    int i      = window_byte & 0x1f;
    tx_window_ = (1 << i) - 1;

    logger::debug() << "Transmit window change " << dec << old_window << "->" << tx_window_
                    << ", in use " << tx_inflight_;

    if (tx_window_ > old_window) {
        tx_enqueue_channel(/*immediate:*/ true);
    }
}

void
base_stream::connect_to(string const& service, string const& protocol)
{
    logger::debug() << "Connecting base stream to " << service << ":" << protocol;

    assert(!service.empty());
    assert(state_ == state::created);
    assert(tx_current_attachment_ == nullptr);

    // Find or create the Target struct for the destination ID
    top_level_ = true;

    // Queue up a service connect message onto the new stream.
    // This will only go out once we actually attach to a channel,
    // but the client can immediately enqueue application data behind it.
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << service_code::connect_request << service << protocol;
    }
    write_record(msg);

    // Record that we're waiting for a response from the server.
    state_ = state::wait_service;

    // Attach to a suitable channel, initiating a new one if necessary.
    attach_for_transmit();
}

/// Get list of services on the peer host.
// future<vector<pair<string,string>>>
// base_stream::get_service_list()
// {
// fixme: stream may be in created or wait_service or connected state, because we might
// want to inspect available services before connecting.
// need to extend state space to include wait_service_list, wait_protocol_list but only
// if it's not waiting service or ready yet?? gets complicated
// can be at once in all three states: waiting service, waiting service list and waiting protocol
// list
// }

/// Get list of protocols for given service on the peer host.
// future<vector<pair<string,string>>>
// base_stream::get_protocol_list(string service)
// {}

void
base_stream::attach_for_transmit()
{
    /// @fixme
    /*
    assert(!peer_id_.is_null());

    // If we already have a transmit-attachment, nothing to do.
    if (tx_current_attachment_ != nullptr) {
        assert(tx_current_attachment_->is_in_use());
        logger::debug() << "Base stream already has attached, doing nothing";
        return;
    }

    // If we're disconnected, we'll never need to attach again...
    if (state_ == state::disconnected) {
        return;
    }

    logger::debug() << "Base stream attaching for transmission";

    // --- connect channel ---

    // See if there's already an active channel for this peer.
    // If so, use it - otherwise, create new one.
    if (!peer_->primary_channel_) {
        // Get the channel setup process for this host ID underway.
        // XXX provide an initial packet to avoid an extra RTT!
        logger::debug() << "Waiting for channel";
        peer_->on_channel_connected.connect([this]() { channel_connected(); });
        return peer_->connect_channel();
    }

    // --- channel connected ---

    stream_channel* channel = peer_->primary_channel_;
    assert(channel->is_active());

    // If we're initiating a new stream and our peer hasn't acked it yet,
    // make sure we have a parent USID to refer to in creating the stream.
    if (init_ and parent_usid_.is_empty())
    {
        auto parent = parent_.lock();
        // No parent USID yet - try to get it from the parent stream.
        if (!parent)
        {
            // Top-level streams just use channel's root stream.
            if (top_level_) {
                parent_ = channel->root_stream();
                parent = parent_.lock();
            } else {
                return fail("Parent stream closed before child stream could be initiated");
            }
        }
        parent_usid_ = parent->usid_;
        // Parent itself doesn't have an USID yet - we have to wait until it does.
        if (parent_usid_.is_empty())
        {
            logger::debug() << "Parent of " << this << " has no USID yet - waiting";
            parent->on_attached.connect([this]() { parent_attached(); });
            return parent->attach_for_transmit();
        }
    }

    //-----------------------------------------
    // Allocate a new USID for this stream.
    //-----------------------------------------

    // Scan forward through our SID space a little ways for a free SID;
    // if there are none, then pick a random one and detach it.
    counter_t sid = channel->allocate_transmit_sid();

    // Find a free attachment slot.
    int slot = 0;
    while (tx_attachments_[slot].is_in_use())
    {
        if (++slot == max_attachments) {
            logger::fatal() << "Base stream attach_for_transmit - all slots are in use.";
            // @todo: Free up some slot.
        }
    }

    // Attach to the stream using the selected slot.
    tx_attachments_[slot].set_attaching(channel, sid);
    tx_current_attachment_ = &tx_attachments_[slot];

    // Fill in the new stream's USID, if it doesn't have one yet.
    if (usid_.is_empty())
    {
        set_usid(unique_stream_id_t(sid, channel->tx_channel_id()));
        logger::debug() << "Creating stream " << usid_;
    }

    // Get us in line to transmit on the channel.
    // We at least need to transmit an attach message of some kind;
    // in the case of Init or Reply it might also include data.

    assert(!contains(channel->sending_streams_, this));
    tx_enqueue_channel();
    if (channel->may_transmit()) {
        channel->on_ready_transmit();
    }
    */
}

void
base_stream::set_usid(unique_stream_id_t new_usid)
{
    assert(usid_.is_empty());
    assert(!new_usid.is_empty());

    if (contains(peer_->usid_streams_, new_usid)) {
        logger::warning() << "Base stream set_usid passed a duplicate stream USID " << new_usid;
    }

    usid_ = new_usid;
    peer_->usid_streams_.insert(make_pair(usid_, shared_from_this()));
}

//-------------------------------------------------------------------------------------------------
// Reading and writing application data.
//-------------------------------------------------------------------------------------------------

ssize_t
base_stream::bytes_available() const
{
    return rx_available_;
}

bool
base_stream::at_end() const
{
    return end_read_; // @todo separate read and write end markers?
}

ssize_t
base_stream::read_data(char* data, ssize_t max_size)
{
    ssize_t actual_size = 0;

    while (max_size > 0 and rx_available_ > 0) {
        assert(!end_read_);
        assert(!rx_segments_.empty());
        rx_segment_t rseg = rx_segments_.front();
        rx_segments_.pop_front();

        ssize_t size = rseg.segment_size();
        assert(size >= 0);

        // @fixme BUG: this breaks if we try to read a partial segment!
        assert(max_size >= size);

        // Copy the data (or just drop it if data == nullptr).
        if (data != nullptr) {
            /// @todo khustup
            // memcpy(data, rseg.buf.data() + rseg.header_len, size);
            data += size;
        }
        actual_size += size;
        max_size -= size;

        // Adjust the receive stats
        rx_available_ -= size;
        rx_buffer_used_ -= size;
        assert(rx_available_ >= 0);

        if (has_pending_records()) {
            // We're reading data from a queued message.
            ssize_t& headsize = rx_record_sizes_.front();
            headsize -= size;
            assert(headsize >= 0);

            // Always stop at the next message boundary.
            if (headsize == 0) {
                rx_record_sizes_.pop_front();
                break;
            }
        } else {
            // No queued messages - just read raw data.
            rx_record_available_ -= size;
            assert(rx_record_available_ >= 0);
        }

        // If this segment has the end-marker set, that's it...
        /// @todo khustup
        // if (rseg.flags() & flags::data_close) {
        shutdown(stream::shutdown_mode::read);
        //}
    }

    // Recalculate the receive window, now that we've (presumably) freed some buffer space.
    recalculate_receive_window();

    return actual_size;
}

ssize_t
base_stream::read_record(char* data, ssize_t max_size)
{
    if (!has_pending_records()) {
        return -1; // No complete records available
    }

    // Read as much of the next queued message as we have room for
    size_t rx_message_count_before = rx_record_sizes_.size();
    ssize_t actual_size = base_stream::read_data(data, max_size);
    assert(actual_size > 0);

    // If the message is longer than the supplied buffer, drop the rest.
    if (rx_record_sizes_.size() == rx_message_count_before) {
        ssize_t skip_size = base_stream::read_data(nullptr, 1 << 30);
        assert(skip_size > 0);
        (void)skip_size;
    }
    assert(rx_record_sizes_.size() == rx_message_count_before - 1);

    return actual_size;
}

byte_array
base_stream::read_record(ssize_t max_size)
{
    ssize_t rec_size = pending_record_size();
    if (rec_size <= 0) {
        return byte_array(); // No complete messages available
    }

    // Read the next message into a new byte array
    byte_array buf;
    ssize_t buf_size = min(rec_size, max_size);
    buf.resize(buf_size);

    ssize_t actual_size = read_record(buf.data(), buf_size);
    assert(actual_size == buf_size);
    (void)actual_size;

    return buf;
}

/** @todo writing data only fills the send buffer_ */
ssize_t
base_stream::write_data(char const* data, ssize_t total_size, uint8_t endflags)
{
    assert(!end_write_);
    ssize_t actual_size = 0;

    do {
        // Choose the size of this segment.
        ssize_t size = mtu;
        // uint8_t flags = 0;

        if (total_size <= size) {
            // flags = flags::data_push | endflags;
            size = total_size;
        }

        logger::debug() << "Transmit segment at [byteseq " << tx_byte_seq_ << "], size " << size
                        << " bytes";

        // Build the appropriate packet header.
        tx_frame_t p(this, frame_type::STREAM);
        p.tx_byte_seq_ = tx_byte_seq_;

        // Prepare the header
        // Accomodate buffer space for payload
        /// @todo khustup
        // p.payload_.resize(channel::header_len + sizeof(data_header) + size);
        // auto header = p.header<data_header>();

        // header->stream_id - later
        // header->type_subtype = flags; // Major type filled in later
        // header->window - later
        // header->tx_byte_seq - later

        // Advance the byte sequence to account for this data.
        tx_byte_seq_ += size;

        // Copy in the application payload
        // char* payload = reinterpret_cast<char*>(header + 1);
        // memcpy(payload, data, size);

        // Hold onto the packet data until it gets ACKed
        tx_waiting_ack_.insert(p.tx_byte_seq_);
        tx_waiting_size_ += size;

        logger::debug() << "write_data inserted [byteseq " << p.tx_byte_seq_
                        << "] into waiting ack, size " << size << ", new count "
                        << tx_waiting_ack_.size() << ", twaitsize " << tx_waiting_size_;

        // Queue up the segment for transmission ASAP
        tx_enqueue_packet(p);

        // On to the next segment...
        data += size;
        total_size -= size;
        actual_size += size;
    } while (total_size > 0);

    // if (endflags & flags::data_close)
    // end_write_ = true;

    return actual_size;
}

//-------------------------------------------------------------------------------------------------
// Unreliable datagrams
//-------------------------------------------------------------------------------------------------

abstract_stream_ptr
base_stream::get_datagram()
{
    // Scan through the list of queued datagrams
    // for one with a complete record waiting to be read.
    for (size_t i = 0; i < received_datagrams_.size(); i++) {
        auto sub = received_datagrams_[i];
        if (!sub->has_pending_records())
            continue;
        received_datagrams_.erase(received_datagrams_.begin() + i);
        return sub;
    }

    set_error("No datagrams available for reading");
    return nullptr;
}

ssize_t
base_stream::read_datagram(char* data, ssize_t max_size)
{
    auto sub = get_datagram();
    if (!sub)
        return -1;

    ssize_t actual_size = sub->read_data(data, max_size);
    sub->shutdown(stream::shutdown_mode::reset); // sub will self-destruct
    return actual_size;
}

byte_array
base_stream::read_datagram(ssize_t max_size)
{
    auto sub = get_datagram();
    if (!sub)
        return byte_array();

    byte_array data = sub->read_record(max_size);
    sub->shutdown(stream::shutdown_mode::reset); // sub will self-destruct
    return data;
}

ssize_t
base_stream::write_datagram(const char* data, ssize_t total_size, stream::datagram_type is_reliable)
{
    logger::debug() << "Sending datagram, size " << total_size << ", "
                    << (is_reliable == stream::datagram_type::reliable ? "reliable" : "unreliable");
    if (is_reliable == stream::datagram_type::reliable
        /*or total_size > (ssize_t)max_stateless_datagram_size*/) {
        // Datagram too large to send using the stateless optimization:
        // just send it as a regular substream.
        logger::debug() << "Sending large datagram, size " << total_size;
        auto sub = open_substream();
        if (sub == nullptr)
            return -1;

        return 0; // sub->write_data(data, total_size, flags::data_close);
        // sub will self-destruct when sent and acked.
    }

    ssize_t remain = total_size;
    // uint8_t flags  = 0; // flags::datagram_begin;
    do {
        // Choose the size of this fragment.
        ssize_t size = mtu;
        if (remain <= size) {
            // flags |= flags::datagram_end;
            size = remain;
        }

        // Build the appropriate packet header.
        /// @todo khustup
        // packet p(this, packet_type::datagram);
        tx_frame_t p(this, frame_type::EMPTY);

        // Assign this packet an ASN as if it were a data segment,
        // but don't actually allocate any TSN bytes to it -
        // this positions the datagram in FIFO order in tx_queue_.
        // XX is this necessarily what we actually want?
        p.tx_byte_seq_ = tx_byte_seq_;

        // Build the datagram header.
        ///@todo khustup
        // p.payload.resize(channel::header_len + sizeof(datagram_header) + size);
        // auto header = p.header<datagram_header>();

        // header->stream_id - later
        /// @todo khustup
        // header->type_subtype = type_and_subtype(packet_type::datagram, flags);
        // header->window - later

        // Copy in the application payload
        // char* payload = reinterpret_cast<char*>(header + 1);
        // memcpy(payload, data, size);

        // Queue up the packet
        tx_enqueue_packet(p);

        // On to the next fragment...
        data += size;
        remain -= size;
        // flags &= ~flags::datagram_begin;
    } while (remain > 0);

    // assert(flags & flags::datagram_end);

    // Once we've enqueued all the fragments of the datagram,
    // add our stream to our flow's transmit queue,
    // and start transmitting immediately if possible.
    tx_enqueue_channel(/*tx_immediately:*/ true);

    return total_size;
}

//-------------------------------------------------------------------------------------------------

void
base_stream::set_priority(priority_t priority)
{
    if (current_priority() != priority) {
        super::set_priority(priority);

        if (tx_enqueued_channel_) {
            stream_channel* chan = tx_current_attachment_->channel_;
            assert(chan->is_active());

            chan->dequeue_stream(this);
            chan->enqueue_stream(this);
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Substreams.
//-------------------------------------------------------------------------------------------------

shared_ptr<abstract_stream>
base_stream::open_substream()
{
    logger::debug() << "Base stream open substream";

    // Create a new sub-stream.
    // Note that the parent doesn't have to be attached yet:
    // the substream will attach and wait for the parent if necessary.
    auto new_stream    = create(host_, peer_id_, shared_from_this());
    new_stream->state_ = state::connected;
    new_stream->self_  = new_stream; // UGH! :(

    // Start trying to attach the new stream, if possible.
    new_stream->attach_for_transmit();

    return new_stream;
}

shared_ptr<abstract_stream>
base_stream::accept_substream()
{
    logger::debug() << "Base stream accept substream";

    if (received_substreams_.empty())
        return nullptr;

    auto sub = received_substreams_.front();
    received_substreams_.pop_front();

    // sub->on_ready_read_record.disconnect(boost::bind(&base_stream::substream_read_record, this));

    return sub;
}

//-------------------------------------------------------------------------------------------------

void
base_stream::set_receive_buffer_size(size_t size)
{
    if (size < min_receive_buffer_size) {
        logger::warning() << "Child receive buffer size " << dec << size << " too small";
        size = min_receive_buffer_size;
    }
    logger::debug() << "Setting base stream receive buffer size " << dec << size << " bytes";
    receive_buf_size_ = size;
}

void
base_stream::set_child_receive_buffer_size(size_t size)
{
    if (size < min_receive_buffer_size) {
        logger::warning() << "Child receive buffer size " << dec << size << " too small";
        size = min_receive_buffer_size;
    }
    logger::debug() << "Setting base stream child receive buffer size " << dec << size << " bytes";
    child_receive_buf_size_ = size;
}

void
base_stream::shutdown(stream::shutdown_mode mode)
{
    logger::debug() << "Shutting down base stream " << this;

    // @todo self-destruct when done, if appropriate

    // @fixme clean this flag mess up
    uint8_t fmode = to_underlying(mode);

    if (fmode & to_underlying(stream::shutdown_mode::reset))
        return disconnect(); // No graceful close necessary

    if (is_link_up() and !end_read_ and (fmode & to_underlying(stream::shutdown_mode::read))) {
        // Shutdown for reading
        rx_available_        = 0;
        rx_record_available_ = 0;
        rx_buffer_used_ = 0;
        readahead_.clear();
        rx_segments_.clear();
        rx_record_sizes_.clear();
        end_read_ = true;
    }

    if (is_link_up() and !end_write_ and (fmode & to_underlying(stream::shutdown_mode::write))) {
        // Shutdown for writing
        // write_data(nullptr, 0, flags::data_close);
    }
}

void
base_stream::disconnect()
{
    logger::debug() << "Disconnecting base stream";
    state_ = state::disconnected;
    // @todo bring down the connection - clear()

    if (auto stream = owner_.lock()) {
        stream->on_link_down();
        // @todo stream->reset()?
    }
}

void
base_stream::fail(string const& error)
{
    disconnect();
    set_error(error);
    logger::warning() << error;
}

void
base_stream::dump()
{
    logger::debug() << "Base stream " << this << " state " << int(state_) << " TSN " << tx_byte_seq_
                    << " RSN " << rx_byte_seq_ << " rx_avail " << rx_available_ << " readahead "
                    << readahead_.size() << " rx_segs " << rx_segments_.size() << " rx_rec_avail "
                    << rx_record_available_ << " rx_recs " << rx_record_sizes_.size();
}

//-------------------------------------------------------------------------------------------------
// Packet transmission
//-------------------------------------------------------------------------------------------------

void
base_stream::tx_enqueue_packet(tx_frame_t& p)
{
    // Add the packet to our stream-local transmit queue.
    // Keep packets in order of transmit sequence number,
    // but in FIFO order for packets with the same sequence number.
    // This happens because datagram packets get assigned the current TSN
    // when they are queued, but without actually incrementing the TSN,
    // just to keep them in the right order with respect to segments.
    // (The assigned TSN is not transmitted in the datagram, of course).
    auto it = tx_queue_.begin();
    while (it != tx_queue_.end() and ((*it).tx_byte_seq_ - p.tx_byte_seq_) <= 0)
        ++it;
    tx_queue_.insert(it, p);

    tx_enqueue_channel(/*immediately:*/ true);
}

void
base_stream::tx_enqueue_channel(bool tx_immediately)
{
    if (!is_attached()) {
        return attach_for_transmit();
    }

    logger::debug(200) << "Base stream enqueue on channel";

    stream_channel* channel = tx_current_attachment_->channel_;
    assert(channel and channel->is_active());

    if (!tx_enqueued_channel_) {
        if (tx_queue_.empty()) {
            if (auto stream = owner_.lock()) {
                stream->on_ready_write();
            }
        } else {
            channel->enqueue_stream(this);
            tx_enqueued_channel_ = true;
        }
    }

    if (tx_immediately and channel->may_transmit()) {
        channel->got_ready_transmit();
    }
}

void
base_stream::tx_attach()
{
    logger::debug() << "Base stream tx_attach";

    stream_channel* chan = tx_current_attachment_->channel_;
    unsigned slot = tx_current_attachment_ - tx_attachments_; // either 0 or 1
    assert(slot < max_attachments);

    // Build the Attach packet header
    tx_frame_t p(this, frame_type::STREAM);
    /// @todo khustup
    // packet p(this, packet_type::attach);
    // auto header = p.header<attach_header>();

    // header->stream_id    = tx_current_attachment_->stream_id_;
    // header->type_subtype = type_and_subtype(
    // frame_type::STREAM, (init_ ? flags::attach_init : 0) | (slot & flags::attach_slot_mask));
    // header->window = receive_window_byte();

    // The body of the Attach packet is the stream's full USID,
    // and the parent's USID too if we're still initiating the stream.
    byte_array body;
    {
        byte_array_owrap<flurry::oarchive> write(body);
        write.archive() << usid_;
        if (init_)
            write.archive() << parent_usid_;
        else
            write.archive() << nullptr;
    }
    /// @todo khustup
    // p.payload_.append(body);

    // Transmit it on the current channel.
    packet_seq_t pktseq;
    /// @todo khustup
    // chan->channel_transmit(p.payload_, pktseq);

    // Save the attach packet in the channel's waiting_ack_ hash,
    // so that we'll be notified when the attach packet gets acked.
    p.late = false;
    chan->waiting_ack_.insert(make_pair(pktseq, p));
}

void
base_stream::tx_attach_data(frame_type type, local_stream_id_t ref_sid)
{
    auto p = tx_queue_.front();
    tx_queue_.pop_front();

    assert(p.type() == frame_type::STREAM);
    assert(p.tx_byte_seq_ <= 0xffff);

    // Build the init_header.
    // auto header       = p.header<init_header>();
    // header->stream_id = tx_current_attachment_->stream_id_;
    // Preserve flags already set.
    // header->type_subtype  = type_and_subtype(type, header->type_subtype); //@fixme &
    // dataAllFlags);
    // header->window        = receive_window_byte();
    // header->new_stream_id = ref_sid;
    // header->tx_seq_no     = p.tx_byte_seq; // Note: 16-bit TSN

    logger::debug() << p;

    // Transmit
    return tx_data(p);
}

void
base_stream::tx_data(tx_frame_t& p)
{
    stream_channel* channel = tx_current_attachment_->channel_;

    // Transmit the packet on our current channel.
    packet_seq_t pktseq;
    channel->channel_transmit(p.payload_, pktseq);

    logger::debug() << "tx_data " << pktseq << " pos " << p.tx_byte_seq_ << " size "
                    << boost::asio::buffer_size(p.payload_);

    // Save the data packet in the channel's ackwait hash.
    p.late = false;
    channel->waiting_ack_.insert(make_pair(pktseq, p));

    // Re-queue us on our channel immediately if we still have more data to send.
    if (tx_queue_.empty()) {
        if (auto stream = owner_.lock()) {
            stream->on_ready_write();
        }
    } else {
        tx_enqueue_channel();
    }
}

void
base_stream::tx_datagram()
{
    logger::debug() << "Base stream tx_datagram";

    // Transmit the whole datagram immediately,
    // so that all fragments get consecutive packet sequence numbers.
    while (true) {
        assert(!tx_queue_.empty());
        tx_frame_t p = tx_queue_.front();
        tx_queue_.pop_front();
        // assert(p.type() == packet_type::datagram);

        // auto header       = p.header<datagram_header>();
        // bool at_end       = (header->type_subtype & flags::datagram_end) != 0;
        // header->stream_id = tx_current_attachment_->stream_id_;
        // header->window    = receive_window_byte();

        // Adjust the in-flight byte count.
        // We don't need to register datagram packets in tx_inflight_
        // because we don't keep them around after they're "missed" -
        // which is fortunate since we _can't_ register them
        // because they don't have unique TSNs.
        tx_inflight_ += p.payload_size();

        // Transmit this datagram packet, but don't save it anywhere - just fire & forget.
        packet_seq_t pktseq;
        tx_current_attachment_->channel_->channel_transmit(p.payload_, pktseq);

        // if (at_end)
        // break;
    }

    // Re-queue us on our channel immediately if we still have more data to send.
    return tx_enqueue_channel();
}

void
base_stream::tx_reset(stream_channel* channel, local_stream_id_t sid, uint8_t flags)
{
    logger::warning() << "Base stream tx_reset";

    // Build the Reset packet header
    // tx_frame_t p(nullptr, packet_type::reset);
    // auto header = p.header<reset_header>();

    // header->stream_id    = sid;
    // header->type_subtype = type_and_subtype(packet_type::reset, flags);
    // header->window       = 0; // No space

    // Transmit it on the current channel.
    // packet_seq_t pktseq;
    // channel->channel_transmit(p.payload_, pktseq);

    // Save the attach packet in the channel's waiting_ack_ hash,
    // so that we'll be notified when the attach packet gets acked.
    // XXX for the packets with O flag set, we don't need to ack
    // if (!(flags & flags::reset_remote_sid)) {
    //     p.late = false;
    //     channel->waiting_ack_.insert(make_pair(pktseq, p));
    // }

    logger::debug() << "Reset packet sent, garbage collecting the stream!";

    // shutdown(stream::shutdown_mode::reset);
    // if (auto stream = owner_.lock()) {
    // stream->on_reset_notify();
    // }
    // self_.reset();

    // as per the PDF:
    // As in TCP, either host may unilaterally terminate an SST stream in both directions and
    // discard
    // any buffered data. A host resets a stream by sending a Reset packet (Figure 6) containing
    // an LSID in either the sender’s or receiver’s LSID space, and an O (Orientation) flag
    // indicating
    // in which space the LSID is to be interpreted. When a host uses a Reset packet to terminate
    // a stream it believes to be active, it uses its own LSID referring to the stream, and resends
    // the Reset packet as necessary until it obtains an acknowledgment. A host also sends a Reset
    // in response to a packet it receives referring to an unknown LSID or USID. This situation
    // may occur if the host has closed and garbage collected its state for a stream but one of its
    // acknowledgments to its peer’s data segments is lost in transit, causing its peer to
    // retransmit
    // those segments. The stateless Reset response indicates to the peer that it can garbage
    // collect
    // its stream state as well. Stateless Reset responses always refer to the peer’s LSID space,
    // since by definition the host itself does not have an LSID assigned to the unknown stream.
}

void
base_stream::acknowledged(stream_channel* channel, tx_frame_t const& pkt, packet_seq_t rx_seq)
{
    logger::debug() << "Base stream ACKed packet of size " << dec << pkt.payload_size();

    switch (pkt.type()) {
        case frame_type::EMPTY:
        case frame_type::PADDING:
        case frame_type::SETTINGS:
        case frame_type::STREAM:
        case frame_type::DETACH:
        case frame_type::DECONGESTION:
        case frame_type::RESET:
        case frame_type::CLOSE:
        case frame_type::ACK:
        case frame_type::PRIORITY:
            break;
            /// @todo
            /*
            case packet_type::data:
                // Mark the segment no longer "in flight".
                end_flight(pkt);

                // Record this TSN as having been ACKed (if not already),
                // so that we don't spuriously resend it
                // if another instance is back in our transmit queue.
                if (contains(tx_waiting_ack_, pkt.tx_byte_seq))
                {
                    tx_waiting_ack_.erase(pkt.tx_byte_seq);
                    tx_waiting_size_ -= pkt.payload_size();

                    logger::debug() << "tx_waiting_ack remove " << pkt.tx_byte_seq
                             << ", size " << pkt.payload_size()
                             << ", new wait count " << tx_waiting_ack_.size()
                             << ", waiting to ack " << tx_waiting_size_
                             << " bytes";
                }
                assert(tx_waiting_size_ >= 0);
                if (auto stream = owner_.lock()) {
                    stream->on_bytes_written(pkt.payload_size()); // XXX delay and coalesce signal
                }

                // fall through...

            case packet_type::attach:
                if (tx_current_attachment_
                    and tx_current_attachment_->channel_ == channel
                    and !tx_current_attachment_->is_acknowledged())
                {
                    // We've gotten our first ack for a new attachment.
                    // Save the rxseq the ack came in on as the attachment's reference pktseq.
                    logger::debug() << "Got attach ack " << rx_seq;
                    tx_current_attachment_->set_active(rx_seq);
                    init_ = false;

                    // Normal data transmission may now proceed.
                    tx_enqueue_channel();

                    // Notify anyone interested that we're attached.
                    on_attached();
                    auto stream = owner_.lock();
                    if (stream and state_ == state::connected) {
                        stream->on_link_up();
                    }
                }
                break;

            case packet_type::ack:
                // nothing to do
                break;

            case packet_type::datagram:
                tx_inflight_ -= pkt.payload_size();  // no longer "in flight"
                assert(tx_inflight_ >= 0);
                break;

            case packet_type::detach:
            case packet_type::reset:
            default:
                logger::warning() << "Got ACK for unknown packet type " << int(pkt.type());
                break;
            */
    }
}

bool
base_stream::missed(stream_channel* channel, tx_frame_t const& pkt)
{
    assert(pkt.late);

    logger::debug() << "Base stream missed seq " << pkt.tx_byte_seq_ << " of size "
                    << pkt.payload_size();

    switch (pkt.type()) {
        case frame_type::EMPTY: {
            logger::debug() << "Retransmit seq " << pkt.tx_byte_seq_ << " of size "
                            << pkt.payload_size();
            // Mark the segment no longer "in flight".
            end_flight(pkt);
            // Retransmit reliable segments...
            tx_frame_t p = pkt;
            tx_enqueue_packet(p);
            return true; // ...but keep the tx record until expiry in case it gets acked late!
        }

        case frame_type::STREAM:
            logger::debug() << "Attach packet lost: trying again to attach";
            tx_enqueue_channel();
            return true;

        case frame_type::ACK:
            logger::debug() << "Datagram packet lost: oops, gone for good";

            // Mark the segment no longer "in flight".
            // We know we'll only do this once per DatagramPacket
            // because we drop it immediately below ("return false");
            // thus acked() cannot be called on it after this.
            tx_inflight_ -= pkt.payload_size(); // no longer "in flight"
            assert(tx_inflight_ >= 0);

            return false;

        case frame_type::DETACH:
        case frame_type::RESET:
        case frame_type::CLOSE:
        default:
            logger::warning() << "Missed unknown packet type " << int(pkt.type());
            return false;
    }
}

void
base_stream::expire(stream_channel* channel, tx_frame_t const& pkt)
{
    // do nothing for now
    // @fixme
}

// Cancel its allocation in our or our parent stream's state,
// according to the type of packet actually sent.
void
base_stream::end_flight(tx_frame_t const& pkt)
{
    // auto header = pkt.header<data_header>();

    // if (type_from_header(header) == packet_type::init) {
    //     if (auto parent = parent_.lock()) {
    //         parent->tx_inflight_ -= pkt.payload_size();
    //         logger::debug() << "Endflight " << pkt.tx_byte_seq << ", bytes in flight on parent "
    //                         << parent->tx_inflight_;
    //         assert(parent->tx_inflight_ >= 0);
    //     }
    // } else { // Reply or Data packet
    //     tx_inflight_ -= pkt.payload_size();
    //     logger::debug() << "Endflight " << pkt.tx_byte_seq << ", bytes in flight " <<
    //     tx_inflight_;
    //     assert(tx_inflight_ >= 0);
    // }
}

//-------------------------------------------------------------------------------------------------
// Packet reception
//-------------------------------------------------------------------------------------------------

// constexpr size_t header_len_min          = channel::header_len + 4;
// constexpr size_t init_header_len_min     = channel::header_len + 8;
// constexpr size_t reply_header_len_min    = channel::header_len + 8;
// constexpr size_t data_header_len_min     = channel::header_len + 8;
// constexpr size_t datagram_header_len_min = channel::header_len + 4;
// constexpr size_t ack_header_len_min      = channel::header_len + 4;
// constexpr size_t reset_header_len_min    = channel::header_len + 4;
// constexpr size_t attach_header_len_min   = channel::header_len + 4;
// constexpr size_t detach_header_len_min   = channel::header_len + 4;

// Main packet receive entry point, called from stream_channel::channel_receive()
bool
base_stream::receive(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < header_len_min) {
    //     logger::warning() << "Base stream - received runt packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // auto header = as_header<stream_header>(pkt);

    // switch (type_from_header(header)) {
    //     case packet_type::init: return rx_init_packet(pktseq, pkt, channel);
    //     case packet_type::reply: return rx_reply_packet(pktseq, pkt, channel);
    //     case packet_type::data: return rx_data_packet(pktseq, pkt, channel);
    //     case packet_type::datagram: return rx_datagram_packet(pktseq, pkt, channel);
    //     case packet_type::ack: return rx_ack_packet(pktseq, pkt, channel);
    //     case packet_type::reset: return rx_reset_packet(pktseq, pkt, channel);
    //     case packet_type::attach: return rx_attach_packet(pktseq, pkt, channel);
    //     case packet_type::detach: return rx_detach_packet(pktseq, pkt, channel);
    //     default:
    //         logger::warning() << "Unknown packet type " << hex << int(type_from_header(header));
    //         return false; // @fixme Protocol error, close channel?
    // }
    return false;
}

bool
base_stream::rx_init_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < init_header_len_min) {
    //     logger::warning() << "Received runt init packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // logger::debug() << "Base stream rx_init_packet";
    // auto header = as_header<init_header>(pkt);

    // // Look up the stream - if it already exists,
    // // just dispatch it directly as if it were a data packet.
    // if (contains(channel->receive_sids_, header->stream_id)) {
    //     logger::debug() << "rx_init_packet: stream exists, dispatch data only";
    //     stream_attachment* attach = channel->receive_sids_[header->stream_id];
    //     if (pktseq < attach->sid_seq_) // earlier init packet; that's OK.
    //         attach->sid_seq_ = pktseq;

    //     channel->ack_sid_ = header->stream_id;
    //     attach->stream_->recalculate_transmit_window(header->window);
    //     attach->stream_->rx_data(pkt, header->tx_seq_no);
    //     return true; // ACK the packet
    // }

    // // Doesn't yet exist - look up the parent stream.
    // if (!contains(channel->receive_sids_, header->new_stream_id)) {
    //     // The parent SID is in error, so reset that SID.
    //     // Ack the pktseq first so peer won't ignore the reset!
    //     logger::warning() << "rx_init_packet: unknown parent stream ID";
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->new_stream_id, flags::reset_remote_sid);
    //     return false;
    // }

    // stream_attachment* parent_attach = channel->receive_sids_[header->new_stream_id];
    // logger::debug() << "rx_init_packet: found parent stream attach " << parent_attach;
    // if (pktseq < parent_attach->sid_seq_) {
    //     logger::warning() << "rx_init_packet: stale wrt parent SID sequence";
    //     return false; // silently drop stale packet
    // }

    // // Extrapolate the sender's stream counter from the new SID it sent,
    // // and use it to form the new stream's USID.
    // counter_t ctr = channel->received_sid_counter_
    //                 + (int16_t)(header->stream_id - (int16_t)channel->received_sid_counter_);
    // unique_stream_id_t usid(ctr, channel->rx_channel_id());

    // logger::debug() << "rx_init_packet: parent attach stream " << parent_attach->stream_;

    // // Create the new substream.
    // auto new_stream =
    //     parent_attach->stream_->rx_substream(pktseq, channel, header->stream_id, 0, usid);

    // if (!new_stream)
    //     return false;

    // // Now process any data segment contained in this init packet.
    // channel->ack_sid_ = header->stream_id;
    // new_stream->recalculate_transmit_window(header->window);
    // new_stream->rx_data(pkt, header->tx_seq_no);

    // return false; // Already acknowledged in rx_substream().
    return false;
}

bool
base_stream::rx_reply_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < reply_header_len_min) {
    //     logger::warning() << "Received runt reply packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // logger::debug() << "Base stream rx_reply_packet";
    // auto header = as_header<reply_header>(pkt);

    // // Look up the stream - if it already exists,
    // // just dispatch it directly as if it were a data packet.
    // if (contains(channel->receive_sids_, header->stream_id)) {
    //     logger::debug() << "rx_reply_packet: stream exists, dispatch data only";
    //     stream_attachment* attach = channel->receive_sids_[header->stream_id];
    //     if (pktseq < attach->sid_seq_) { // earlier reply packet; that's OK.
    //         attach->sid_seq_ = pktseq;
    //     }

    //     channel->ack_sid_ = header->stream_id;
    //     attach->stream_->recalculate_transmit_window(header->window);
    //     attach->stream_->rx_data(pkt, header->tx_seq_no);
    //     return true; // ACK the packet
    // }

    // // Doesn't yet exist - look up the reference stream in our SID space.
    // if (!contains(channel->transmit_sids_, header->new_stream_id)) {
    //     // The reference SID supposedly in our own space is invalid!
    //     // Respond with a reset indicating that SID in our space.
    //     // Ack the pktseq first so peer won't ignore the reset!
    //     logger::debug() << "rx_reply_packet: unknown reference stream ID";
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->new_stream_id, 0);
    //     return false;
    // }

    // stream_attachment* attach = channel->transmit_sids_[header->new_stream_id];

    // if (pktseq < attach->sid_seq_) {
    //     logger::debug() << "rx_reply_packet: stale packet - pktseq " << pktseq << " sidseq "
    //                     << attach->sid_seq_;
    //     return false; // silently drop stale packet
    // }

    // base_stream* stream = attach->stream_;

    // logger::debug() << stream << " Accepting reply " << stream->usid_;

    // // OK, we have the stream - just create the receive-side attachment.
    // stream->rx_attachments_[0].set_active(channel, header->stream_id, pktseq);

    // // Now process any data segment contained in this reply packet.
    // channel->ack_sid_ = header->stream_id;
    // stream->recalculate_transmit_window(header->window);
    // stream->rx_data(pkt, header->tx_seq_no);

    // return true; // Acknowledge the packet
    return false;
}

bool
base_stream::rx_data_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < data_header_len_min) {
    //     logger::warning() << "Received runt data packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // logger::debug() << "Base stream rx_data_packet";
    // auto header = as_header<data_header>(pkt);

    // if (!contains(channel->receive_sids_, header->stream_id)) {
    //     // Respond with a reset for the unknown stream ID.
    //     // Ack the pktseq first so peer won't ignore the reset!
    //     logger::debug() << "rx_data_packet: unknown stream ID " << header->stream_id;
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    //     return false;
    // }

    // stream_attachment* attach = channel->receive_sids_[header->stream_id];
    // if (pktseq < attach->sid_seq_) {
    //     logger::debug() << "rx_data_packet: stale packet - pktseq " << pktseq << " sidseq "
    //                     << attach->sid_seq_;
    //     return false; // silently drop stale packet
    // }

    // // Process the data, using the full 32-bit TSN.
    // channel->ack_sid_ = header->stream_id;
    // attach->stream_->recalculate_transmit_window(header->window);
    // attach->stream_->rx_data(pkt, header->tx_seq_no);
    // return true; // ACK the packet
    return false;
}

bool
base_stream::rx_datagram_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < datagram_header_len_min) {
    //     logger::warning() << "Received runt datagram packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // logger::debug() << "Base stream rx_datagram_packet";
    // auto header = as_header<datagram_header>(pkt);

    // // Look up the stream for which the datagram is a substream.
    // if (!contains(channel->receive_sids_, header->stream_id)) {
    //     // Respond with a reset for the unknown stream ID.
    //     // Ack the pktseq first so peer won't ignore the reset!
    //     logger::warning() << "rx_datagram_packet: unknown stream ID " << header->stream_id;
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    //     return false;
    // }

    // stream_attachment* attach = channel->receive_sids_[header->stream_id];

    // channel->ack_sid_ = header->stream_id; // @fixme Why do we update ack_sid here?

    // if (pktseq < attach->sid_seq_) {
    //     logger::debug() << "rx_datagram_packet: stale packet - pktseq " << pktseq << " sidseq "
    //                     << attach->sid_seq_;
    //     return false; // silently drop stale packet
    // }

    // base_stream* base = attach->stream_;

    // if (base->state_ != state::connected) {
    //     // Only accept datagrams while connected
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    //     return false;
    // }

    // int flags = header->type_subtype;

    // if (!(flags & flags::datagram_begin) or !(flags & flags::datagram_end)) {
    //     // @todo Fix datagram reassembly.
    //     logger::fatal() << "OOPS, don't yet know how to reassemble datagrams";
    //     return false;
    // }

    // // Build a pseudo-Stream object encapsulating the datagram.
    // auto dgram = make_shared<datagram_stream>(base->host_, pkt, datagram_header_len_min);
    // base->received_datagrams_.push_back(dgram);

    // // Don't need to connect to the sub's on_ready_read_record() signal
    // // because we already know the sub is completely received...
    // if (auto stream = base->owner_.lock()) {
    //     stream->on_ready_read_datagram();
    // }

    // return true; // Acknowledge the packet
    return false;
}

bool
base_stream::rx_ack_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < ack_header_len_min) {
    //     logger::warning() << "Received runt ack packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // // Count this explicit ack packet as received,
    // // but do NOT send another ack just to ack this ack!
    // channel->acknowledge(pktseq, false);

    // logger::debug() << "Base stream rx_ack_packet";
    // auto header = as_header<ack_header>(pkt);

    // // Look up the stream the data packet belongs to.
    // // The SID field in an Ack packet is in our transmit SID space,
    // // because it reflects data our peer is receiving.
    // if (!contains(channel->transmit_sids_, header->stream_id)) {
    //     // The reference SID supposedly in our own space is invalid!
    //     // Respond with a reset indicating that SID in our space.
    //     // Ack the pktseq first so peer won't ignore the reset!
    //     logger::debug() << "rx_ack_packet: unknown stream ID " << header->stream_id;
    //     channel->acknowledge(pktseq, false);
    //     tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    //     return false;
    // }

    // stream_attachment* attach = channel->transmit_sids_[header->stream_id];

    // if (pktseq < attach->sid_seq_) {
    //     logger::debug() << "rx_ack_packet: stale packet - pktseq " << pktseq << " sidseq "
    //                     << attach->sid_seq_;
    //     return false; // silently drop stale packet
    // }

    // // Process the transmit window update.
    // attach->stream_->recalculate_transmit_window(header->window);

    // return false; // Do not acknowledge.
    return false;
}

/**
 * @todo Received a reset packet, forcefully reset stream.
 * @param  pktseq  Packet sequence number.
 * @param  pkt     Reset packet itself.
 * @param  channel Associated channel.
 * @return         true if reset is successful.
 */
bool
base_stream::rx_reset_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < reset_header_len_min) {
    //     logger::warning() << "Received runt reset packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // logger::warning() << "Base stream rx_reset_packet UNIMPLEMENTED.";
    // // auto header = as_header<reset_header>(pkt);
    // // bool local_sid = hdr->type & flags::reset_remote_sid;
    // //
    // // if sid not found: do nothing
    // //
    // // @todo...
    // //
    // return false;
    return false;
}

bool
base_stream::rx_attach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    // if (pkt.size() < attach_header_len_min) {
    //     logger::warning() << "Received runt attach packet";
    //     return false; // @fixme Protocol error, close channel?
    // }

    // auto header = as_header<attach_header>(pkt);
    // bool init   = (header->type_subtype & flags::attach_init) != 0;
    // int slot    = header->type_subtype & flags::attach_slot_mask;

    // static_assert(flags::attach_slot_mask == max_attachments - 1,
    //               "max_attachments value changed, need to fix some other code too.");

    // unique_stream_id_t usid, parent_usid;

    // logger::debug() << "Base stream received attach packet, " << (init ? "init" : "non-init")
    //                 << " attach on slot " << slot;

    // byte_array_iwrap<flurry::iarchive> read(pkt);
    // read.archive().skip_raw_data(sizeof(attach_header) + channel::header_len);

    // // Decode the USID(s) in the body
    // read.archive() >> usid;
    // if (init) {
    //     read.archive() >> parent_usid;
    // }

    // if (usid.is_empty() or (init and parent_usid.is_empty())) {
    //     logger::warning() << "Invalid attach packet received";
    //     return false; // @fixme Protocol error, shutdown channel?
    // }

    // if (contains(channel->peer_->usid_streams_, usid)) {
    //     // Found it: the stream already exists, just attach it.
    //     base_stream* stream = channel->peer_->usid_streams_[usid];

    //     logger::debug() << "Found USID in existing streams";
    //     channel->ack_sid_           = header->stream_id;
    //     stream_rx_attachment& rslot = stream->rx_attachments_[slot];
    //     if (rslot.is_active()) {
    //         if (rslot.channel_ == channel and rslot.stream_id_ == header->stream_id) {
    //             logger::debug() << stream << " redundant attach " << stream->usid_;
    //             rslot.sid_seq_ = min(rslot.sid_seq_, pktseq);
    //             return true;
    //         }
    //         logger::debug() << stream << " replacing attach slot " << slot;
    //         rslot.clear();
    //     }
    //     logger::debug() << stream << " accepting attach " << stream->usid_;
    //     rslot.set_active(channel, header->stream_id, pktseq);
    //     return true;
    // }

    // for (auto x : channel->peer_->usid_streams_) {
    //     logger::debug() << "known usid " << x.first;
    // }

    // // Stream doesn't exist - lookup parent if it's an init-attach.
    // base_stream* parent_stream{nullptr};

    // if (init and contains(channel->peer_->usid_streams_, parent_usid)) {
    //     parent_stream = channel->peer_->usid_streams_[parent_usid];
    // }

    // if (parent_stream) {
    //     // Found it: create and attach a child stream.
    //     channel->ack_sid_ = header->stream_id;
    //     parent_stream->rx_substream(pktseq, channel, header->stream_id, slot, usid);
    //     // @todo: rx_substream() may fail to create the stream...
    //     return false; // Already acked in rx_substream()
    // }

    // // No way to attach the stream - just reset it.
    // logger::debug() << "rx_attach_packet: unknown stream " << usid;
    // channel->acknowledge(pktseq, false);
    // tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    // return false;
    return false;
}

/**
 * @todo Received a detach packet, disconnect stream from the channel.
 */
bool
base_stream::rx_detach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    return false;
}

void
base_stream::rx_data(byte_array const& pkt, uint32_t byte_seq)
{
    // if (end_read_) {
    //     // Ignore anything we receive past end of stream
    //     // (which we may have forced from our end via close()).
    //     logger::warning() << "Ignoring segment received after end-of-stream";
    //     assert(readahead_.empty());
    //     assert(rx_segments_.empty());
    //     return;
    // }

    // rx_segment_t rseg(pkt, byte_seq, channel::header_len + sizeof(data_header));
    // int seg_size = rseg.segment_size();

    // logger::debug() << "rx_data " << byte_seq << " payload size " << seg_size << " flags "
    //                 << int(rseg.flags()) << " stream rx_seq " << rx_byte_seq_;

    // // See where this packet fits in
    // int rx_seq_diff = rseg.rx_byte_seq - rx_byte_seq_;
    // if (rx_seq_diff <= 0) {
    //     // The segment is at or before our current receive position.
    //     // How much of its data, if any, is actually useful?
    //     // Note that we must process packets at our rx_seq with no data,
    //     // because they might have important flags.
    //     int act_size = seg_size + rx_seq_diff;
    //     if (act_size < 0 or (act_size == 0 and !rseg.has_flags())) {
    //         // The packet is way out of date -
    //         // its end doesn't even come up to our current RSN.
    //         logger::debug() << "Duplicate segment at rx_seq " << rseg.rx_byte_seq << " size "
    //                         << seg_size;
    //         return recalculate_receive_window();
    //     }
    //     rseg.header_len -= rx_seq_diff; // Merge useless data into "headers"
    //     logger::debug() << "actual_size " << act_size << " flags " << int(rseg.flags());

    //     // It gives us exactly the data we want next - very good!
    //     bool was_empty   = !has_bytes_available();
    //     bool was_no_recs = !has_pending_records();
    //     bool closed      = false;

    //     rx_enqueue_segment(rseg, act_size, /*inout*/ closed);

    //     // Then pull anything we can from the reorder buffer
    //     for (; !readahead_.empty(); readahead_.pop_front()) {
    //         rx_segment_t& read_seg = readahead_.front();
    //         int seg_size           = read_seg.segment_size();

    //         int rx_seq_diff = read_seg.rx_byte_seq - rx_byte_seq_;
    //         if (rx_seq_diff > 0) {
    //             break; // There's still a gap
    //         }

    //         // Account for removal of this segment from readhead_;
    //         // below we'll re-add whatever part of it we use.
    //         rx_buffer_used_ -= seg_size;

    //         logger::debug() << "Pull readahead segment at " << read_seg.rx_byte_seq << " of size
    //         "
    //                         << seg_size << " from reorder buffer";

    //         int act_size = seg_size + rx_seq_diff;
    //         if (act_size < 0 or (act_size == 0 and !read_seg.has_flags())) {
    //             continue; // No useful data: drop
    //         }

    //         read_seg.header_len -= rx_seq_diff;

    //         // Consume this segment too.
    //         rx_enqueue_segment(read_seg, act_size, /*inout*/ closed);
    //     }

    //     // If we're at the end of stream with no data to read,
    //     // go into the end-of-stream state immediately.
    //     // We must do this because read_data() may never
    //     // see our queued zero-length segment if rx_available_ == 0.
    //     if (closed and rx_available_ == 0) {
    //         shutdown(stream::shutdown_mode::read);
    //         on_ready_read_record();
    //         auto stream = owner_.lock();
    //         if (is_link_up() and stream) {
    //             stream->on_ready_read();
    //             stream->on_ready_read_record();
    //         }
    //         return recalculate_receive_window();
    //     }

    //     // Notify the client if appropriate
    //     if (was_empty) {
    //         auto stream = owner_.lock();
    //         if (state_ == state::connected and stream) {
    //             stream->on_ready_read();
    //         }
    //     }

    //     if (was_no_recs and has_pending_records()) {
    //         if (state_ == state::connected) {
    //             on_ready_read_record();
    //             if (auto stream = owner_.lock()) {
    //                 stream->on_ready_read_record();
    //             }
    //         } else if (state_ == state::wait_service) {
    //             got_service_reply();
    //         } else if (state_ == state::accepting) {
    //             got_service_request();
    //         }
    //     }
    // } else if (rx_seq_diff > 0) {
    //     // @todo Test this section

    //     // It's out of order beyond our current receive sequence -
    //     // stash it in a re-order buffer, sorted by rx_seq.

    //     logger::debug() << "Received out-of-order segment at " << rseg.rx_byte_seq << " size "
    //                     << seg_size;

    //     // Binary search for the correct position.
    //     // lower_bound() because we want to see if there is the same element already in deque
    //     auto it = lower_bound(
    //         readahead_.begin(),
    //         readahead_.end(),
    //         rx_seq_diff,
    //         [this](rx_segment_t& itr, int val) { return (itr.rx_byte_seq - rx_byte_seq_) < val;
    //         });

    //     // Don't save duplicate segments
    //     // (unless the duplicate actually has more data or new flags).
    //     if (it != readahead_.end() and (*it).rx_byte_seq == rseg.rx_byte_seq
    //         and seg_size <= (*it).segment_size()
    //         and rseg.flags() == (*it).flags()) {
    //         logger::debug() << "rxseg duplicate out-of-order segment - rx_seq " <<
    //         rseg.rx_byte_seq;
    //         return recalculate_receive_window();
    //     }

    //     rx_buffer_used_ += seg_size;
    //     readahead_.insert(it, rseg);
    // }

    // // Recalculate the receive window now that we've probably consumed some buffer space.
    // recalculate_receive_window();
}

base_stream_ptr
base_stream::rx_substream(packet_seq_t pktseq,
                          stream_channel* channel,
                          local_stream_id_t sid,
                          unsigned slot,
                          unique_stream_id_t const& usid)
{
    // Make sure we're allowed to create a child stream.
    if (!is_listening()) {
        // The parent SID is not in error, so just reset the new child.
        // Ack the pktseq first so peer won't ignore the reset!
        logger::warning() << "Other side trying to create substream, but we're not listening.";
        channel->acknowledge(pktseq, false);
        // tx_reset(channel, sid, flags::reset_remote_sid);
        return nullptr;
    }

    // Mark the Init packet received now, before transmitting the Reply;
    // otherwise the Reply won't acknowledge the Init
    // and the peer will reject it as a stale packet.
    channel->acknowledge(pktseq, true);

    // Create the child stream.
    auto new_stream   = create(channel->get_host(), peer_id_, shared_from_this());
    new_stream->self_ = new_stream; // UGH :(

    // We'll accept the new stream: this is the point of no return.
    logger::debug() << "Accepting sub-stream " << usid << " as " << new_stream;

    // Extrapolate the sender's stream counter from the new SID it sent.
    counter_t ctr =
        channel->received_sid_counter_ + (int16_t)(sid - (int16_t)channel->received_sid_counter_);
    if (ctr > channel->received_sid_counter_)
        channel->received_sid_counter_ = ctr;

    // Use this stream counter to form the new stream's USID.
    // @fixme ctr is not used here??
    new_stream->set_usid(usid);

    // Automatically attach the child via its appropriate receive-slot.
    new_stream->rx_attachments_[slot].set_active(channel, sid, pktseq);

    // If this is a new top-level application stream,
    // we expect a service request before application data.
    if (shared_from_this() == channel->root_) {
        new_stream->state_ = state::accepting; // Service request expected on root stream
    } else {
        new_stream->state_ = state::connected;
        received_substreams_.push_back(new_stream);
        // new_stream->on_ready_read_record.connect(
        // boost::bind(&base_stream::substream_read_record, this));
        if (auto stream = owner_.lock()) {
            stream->on_new_substream();
        }
    }

    return new_stream;
}

// Helper function to enqueue useful data segments.
void
base_stream::rx_enqueue_segment(rx_segment_t const& seg, size_t actual_size, bool& closed)
{
    rx_segments_.push_back(seg);
    rx_byte_seq_ += actual_size;
    rx_available_ += actual_size;
    rx_record_available_ += actual_size;
    rx_buffer_used_ += actual_size;

    if (/*(seg.flags() bitand (flags::data_record | flags::data_close))
        and*/ (rx_record_available_
                                                                         > 0)) {
        logger::debug() << "Received complete record";
        rx_record_sizes_.push_back(rx_record_available_);
        rx_record_available_ = 0;
    }
    /*if (seg.flags() & flags::data_close)
        closed = true;*/
}

static inline byte_array
service_reply(stream_protocol::service_code reply, string message)
{
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << stream_protocol::service_code::connect_reply << reply << message;
    }
    return msg;
}

void
base_stream::got_service_request()
{
    assert(state_ == state::accepting);

    byte_array rec = read_record(max_service_record_size);
    logger::debug() << "Received record " << rec;
    byte_array_iwrap<flurry::iarchive> read(rec);
    uint32_t code;
    string service, protocol;
    read.archive() >> code >> service >> protocol; // @fixme may throw..
    if (code != service_code::connect_request)
        return fail("Bad service request");

    logger::debug() << "got_service_request service '" << service << "' protocol '" << protocol
                    << "'";

    // Lookup the requested service
    server* srv = host_->listener_for(service, protocol);

    if (!srv) {
        ostringstream oss;
        oss << "Request for service " << service << " with unknown protocol " << protocol;
        write_record(service_reply(service_code::reply_not_found, oss.str()));
        return fail(oss.str());
    }

    // Send a service reply to the client
    write_record(service_reply(service_code::reply_ok, "ok"));

    // Hand off the new stream to the chosen service
    state_ = state::connected;
    srv->received_connections_.push(shared_from_this());
    srv->on_new_connection();
}

void
base_stream::got_service_reply()
{
    assert(state_ == state::wait_service);
    assert(tx_current_attachment_);

    byte_array rec = read_record(max_service_record_size);
    logger::debug() << "Received record " << rec;

    byte_array_iwrap<flurry::iarchive> read(rec);
    uint32_t code, status;
    string message;
    read.archive() >> code >> status >> message; // @todo Read code separately!
    if (code != service_code::connect_reply or status != 0) {
        ostringstream oss;
        oss << "Service connect failed with code " << code << " status " << status << " message "
            << message;
        return fail(oss.str());
    }

    logger::debug() << "got_service_reply code '" << code << "' status '" << status << "' message '"
                    << message << "'";

    state_ = state::connected;
    if (auto stream = owner_.lock())
        stream->on_link_up();
}

//-----------------
// Signal handlers
//-----------------

void
base_stream::channel_connected()
{
    logger::debug() << "Base stream - channel has connected.";
    if (peer_) {
        peer_->on_channel_connected.disconnect(boost::bind(&base_stream::channel_connected, this));
    }

    // Retry attach now that we hopefully have an active channel.
    attach_for_transmit();
}

void
base_stream::parent_attached()
{
    logger::debug() << "Base stream - parent stream has attached, we can now attach.";
    if (auto parent = parent_.lock()) {
        parent->on_attached.disconnect(boost::bind(&base_stream::parent_attached, this));
    }

    // Retry attach now that parent hopefully has a USID.
    attach_for_transmit();
}

// void base_stream::substream_read_record()
// {
// When one of our queued subs emits an on_ready_read_record() signal,
// we have to forward that via our on_ready_read_datagram() signal.
// @fixme WHY?
// Not sure this is a good idea.
// Basically it boils down to when substream has received a record we consider it to be
// a datagram substream and fire off datagram reading in client, with the new
// received_datagrams_ list it won't work.
// See comment in base_stream.h

// if (auto stream = owner_.lock()) {
// stream->on_ready_read_datagram();
// }
// }

//=================================================================================================
// stream_tx_attachment
// Where our stream attaches to channel.
//=================================================================================================

void
stream_tx_attachment::set_attaching(stream_channel* channel, local_stream_id_t sid)
{
    assert(!is_in_use());

    logger::debug() << "Stream transmit attachment going active on channel " << channel;

    channel_   = channel;
    stream_id_ = sid;
    sid_seq_   = ~0; //@fixme magic number
    active_ = deprecated_ = false;

    assert(!contains(channel_->transmit_sids_, stream_id_));
    channel_->transmit_sids_.insert(make_pair(stream_id_, this));
    logger::debug() << "Stream transmit attachment sid " << stream_id_ << " activated";
}

void
stream_tx_attachment::clear()
{
    stream_channel* channel = channel_;
    if (!channel)
        return;

    if (stream_->tx_current_attachment_ == this)
        stream_->tx_current_attachment_ = nullptr; // @fixme Send notification?

    assert(contains(channel->transmit_sids_, stream_id_));
    assert(channel->transmit_sids_[stream_id_] == this);

    logger::debug() << "Clearing tx attachment for sid " << stream_id_;
    channel->transmit_sids_.erase(stream_id_);
    channel_ = nullptr;
    active_  = false;

    // Remove the stream from the channel's waiting streams list
    channel->dequeue_stream(stream_);
    stream_->tx_enqueued_channel_ = false;

    // Clear out packets for this stream from channel's ackwait table
    logger::debug() << "waiting ack size " << channel->waiting_ack_.size();

    auto ack_copy = channel->waiting_ack_;
    for (auto ackw : ack_copy) {
        base_stream::tx_frame_t& p = ackw.second;
        assert(!p.is_null());

        if (p.owner != stream_)
            continue;

        // Move the packet back to the stream's transmit queue
        if (!p.late) {
            p.late = true;
            stream_->missed(channel, p);
        } else {
            stream_->expire(channel, p);
        }

        channel->waiting_ack_.erase(ackw.first);
        logger::debug() << "Cleared packet";
    }
}

//=================================================================================================
// stream_rx_attachment
// Where the peer's stream is attached to the channel.
//=================================================================================================

void
stream_rx_attachment::set_active(stream_channel* channel, local_stream_id_t sid, packet_seq_t rxseq)
{
    assert(!is_active());

    logger::debug() << "Stream receive attachment going active on channel " << channel;

    channel_   = channel;
    stream_id_ = sid;
    sid_seq_   = rxseq;

    assert(!contains(channel_->receive_sids_, stream_id_));
    channel_->receive_sids_.insert(make_pair(stream_id_, this));
}

void
stream_rx_attachment::clear()
{
    logger::debug() << "Stream receive attachment going inactive";
    if (channel_) {
        assert(contains(channel_->receive_sids_, stream_id_));
        assert(channel_->receive_sids_[stream_id_] == this);
        channel_->receive_sids_.erase(stream_id_);
        channel_ = nullptr;
    }
}

} // sss namespace
