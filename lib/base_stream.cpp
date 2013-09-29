//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "base_stream.h"
#include "logging.h"
#include "host.h"
#include "private/stream_peer.h"
#include "stream_channel.h"
#include "flurry.h"
#include "byte_array_wrap.h"
#include "algorithm.h"
#include "server.h"
#include "pubqueue.h"

using namespace std;

namespace ssu {

//=================================================================================================
// Helper functions.
//=================================================================================================

template <typename T>
inline T const* as_header(byte_array const& v)
{
    return reinterpret_cast<T const*>(v.const_data() + channel::header_len);
}

template <typename T>
inline T* as_header(byte_array& v)
{
    return reinterpret_cast<T*>(v.data() + channel::header_len);
}

//=================================================================================================
// base_stream
//=================================================================================================

constexpr int base_stream::max_attachments;

base_stream::base_stream(shared_ptr<host> host, 
                         const peer_id& peer_id,
                         shared_ptr<base_stream> parent)
    : abstract_stream(host)
    , parent_(parent)
{
    assert(!peer_id.is_empty());

    logger::debug() << "Constructing internal stream for peer " << peer_id;

    // Initialize inherited parameters
    if (parent)
    {
        if (parent->listen_mode() & stream::listen_mode::inherit)
            listen(parent->listen_mode());
        receive_buf_size_ = child_receive_buf_size_ = parent->child_receive_buf_size_;
    }

    recalculate_receive_window();

    peerid_ = peer_id;
    peer_ = host->stream_peer(peer_id);

    // Insert us into the peer's master list of streams
    peer_->all_streams_.insert(this);

    // Initialize the stream back-pointers in the attachment slots.
    for (int i = 0; i < max_attachments; ++i)
    {
        tx_attachments_[i].stream_ = this;
        rx_attachments_[i].stream_ = this;
    }
}

base_stream::~base_stream()
{
    logger::debug() << "Destructing internal stream";
    clear();
}

void base_stream::clear()
{
    state_ = state::disconnected;
    end_read_ = end_write_ = true;

    // De-register us from our peer
    if (peer_)
    {
        if (peer_->usid_streams_.at(usid_) == this)
            peer_->usid_streams_.erase(usid_);
        peer_->all_streams_.erase(this);
        peer_ = nullptr;
    }

    // Clear any attachments we may have
    for (int i = 0; i < max_attachments; ++i)
    {
        tx_attachments_[i].clear();
        rx_attachments_[i].clear();
    }

    // Reset any unaccepted incoming substreams too
    for (auto sub : get_c(received_substreams_))
    {
        sub->shutdown(stream::shutdown_mode::reset);
        // should self-destruct automatically when done
    }
    get_c(received_substreams_).clear();
}

bool base_stream::is_attached()
{
    return tx_current_attachment_ != nullptr;
}

void base_stream::transmit_on(stream_channel* channel)
{
    assert(tx_enqueued_channel_);
    assert(tx_current_attachment_ != nullptr);
    assert(channel == tx_current_attachment_->channel_);
    assert(!tx_queue_.empty());

    logger::debug() << "Internal stream transmit_on " << channel;

    tx_enqueued_channel_ = false; // Channel has just dequeued us.

    // First garbage-collect any segments that have already been ACKed;
    // this can happen if we retransmit a segment but an ACK for the original arrives late.
    packet* head_packet = &tx_queue_.front();

    while (head_packet->type == packet_type::data
        and !contains(tx_waiting_ack_, head_packet->tx_byte_seq))
    {
        // No longer waiting for this tsn - must have been ACKed.
        tx_queue_.pop_front();
        if (tx_queue_.empty())
        {
            if (auto stream = owner_.lock()) {
                stream->on_ready_write();
            }
            return;
        }
        head_packet = &tx_queue_.front();
    }

    int seg_size = head_packet->payload_size();

    // Ensure our attachment has been acknowledged before using the SID.
    if (tx_current_attachment_->is_acknowledged())
    {
        // Our attachment has been acknowledged, send the data packets freely.
        assert(!init_);
        assert(tx_current_attachment_->is_active());

        // Throttle data transmission if channel window is full
        if (tx_inflight_ + seg_size > tx_window_)
        {
            logger::debug() << this << " Transmit window full - need " << (tx_inflight_ + seg_size)
                << " have " << tx_window_;
            // XXX query status if latest update is out-of-date!
            //XXXreturn;
        }

        // Datagrams get special handling.
        if (head_packet->type == packet_type::datagram)
            return tx_datagram();

        // Register the segment as being in-flight.
        tx_inflight_ += seg_size;

        logger::debug() << this << " inflight data " << head_packet->tx_byte_seq
            << ", bytes in flight " << tx_inflight_;

        // Transmit the next segment in a regular Data packet.
        packet p = tx_queue_.front();
        tx_queue_.pop_front();

        assert(p.type == packet_type::data);

        logger::debug() << p;

        auto header = as_header<data_header>(p.buf);
        header->stream_id = tx_current_attachment_->stream_id_;
        // Preserve flags already set.
        header->type_subtype = type_and_subtype(packet_type::data, header->type_subtype); //@fixme & dataAllFlags);
        header->window = receive_window_byte();
        header->tx_seq_no = p.tx_byte_seq; // Note: 32-bit TSN

        // Transmit
        return tx_data(p);
    }

    // See if we can potentially use an optimized attach/data packet;
    // this only works for regular stream segments, not datagrams,
    // and only within the first 2^16 bytes of the stream.
    if (head_packet->type == packet_type::data and
        head_packet->tx_byte_seq <= 0xffff)
    {
        // See if we can attach stream using an optimized Init packet,
        // allowing us to indicate the parent with a short 16-bit LSID
        // and piggyback useful data onto the packet.
        // The parent must be attached to the same channel.
        // XXX probably should use some state invariant
        // in place of all these checks.
        if (top_level_)
            parent_ = channel->root_;

        shared_ptr<base_stream> parent = parent_.lock();

        if (init_ and parent and parent->tx_current_attachment_
                and parent->tx_current_attachment_->channel_ == channel
                and parent->tx_current_attachment_->is_active()
                and usid_.half_channel_id_ == channel->tx_channel_id()
                and uint16_t(usid_.counter_) == tx_current_attachment_->stream_id_
            /* XXX  and parent->tx_inflight_ + seg_size <= parent->tx_window_*/)
        {
            logger::debug() << "Sending optimized init packet with " << seg_size << " payload bytes";

            // Adjust the in-flight byte count for channel control.
            // Init packets get "charged" to the parent stream.
            parent->tx_inflight_ += seg_size;
            logger::debug() << this << " inflight init " << head_packet->tx_byte_seq
                << ", bytes in flight on parent " << parent->tx_inflight_;

            return tx_attach_data(packet_type::init, parent->tx_current_attachment_->stream_id_);
        }

        // See if our peer has this stream in its SID space,
        // allowing us to attach using an optimized Reply packet.
        if (tx_inflight_ + seg_size <= tx_window_)
        {
            for (int i = 0; i < max_attachments; ++i)
            {
                if (rx_attachments_[i].channel_ == channel and rx_attachments_[i].is_active())
                {
                    logger::debug() << "Sending optimized reply packet";

                    // Adjust the in-flight byte count.
                    tx_inflight_ += seg_size;
                    logger::debug() << this << " inflight reply " << head_packet->tx_byte_seq
                        << ", bytes in flight " << tx_inflight_;

                    return tx_attach_data(packet_type::reply, rx_attachments_[i].stream_id_);
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

void base_stream::recalculate_receive_window()
{
    assert(receive_buf_size_ > 0);
    logger::debug() << "Internal stream recalculate receive window";
    receive_window_byte_ = 0x1a;
}

void base_stream::recalculate_transmit_window(uint8_t window_byte)
{
    int32_t old_window = tx_window_;

    // Calculate the new transmit window
    int i = window_byte & 0x1f;
    tx_window_ = (1 << i) - 1;

    logger::debug() << this << " Transmit window change " << old_window << "->" << tx_window_
        << ", in use " << tx_inflight_;

    if (tx_window_ > old_window)
        tx_enqueue_channel(/*immediate:*/true);
}

void base_stream::connect_to(string const& service, string const& protocol)
{
    logger::debug() << "Connecting internal stream to " << service << ":" << protocol;

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

void base_stream::attach_for_transmit()
{
    assert(!peerid_.is_empty());

    // If we already have a transmit-attachment, nothing to do.
    if (tx_current_attachment_ != nullptr) {
        assert(tx_current_attachment_->is_in_use());
        logger::debug() << "Internal stream already has attached, doing nothing";
        return;
    }

    // If we're disconnected, we'll never need to attach again...
    if (state_ == state::disconnected)
        return;

    logger::debug() << "Internal stream attaching for transmission";

    // See if there's already an active channel for this peer.
    // If so, use it - otherwise, create new one.
    if (!peer_->primary_channel_) {
        // Get the channel setup process for this host ID underway.
        // XXX provide an initial packet to avoid an extra RTT!
        logger::debug() << "Waiting for channel";
        peer_->on_channel_connected.connect(boost::bind(&base_stream::channel_connected, this));
        return peer_->connect_channel();
    }

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
            // Top-level streams just use any channel's root stream.
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
            parent->on_attached.connect(boost::bind(&base_stream::parent_attached, this));
            return parent->attach_for_transmit();
        }
    }

    //-----------------------------------------
    // Allocate a stream_id_t for this stream.
    //-----------------------------------------

    // Scan forward through our SID space a little ways for a free SID;
    // if there are none, then pick a random one and detach it.
    counter_t sid = channel->allocate_transmit_sid();

    // Find a free attachment slot.
    int slot = 0;
    while (tx_attachments_[slot].is_in_use())
    {
        if (++slot == max_attachments) {
            logger::fatal() << "attach_for_transmit: all slots are in use.";
            // @fixme: Free up some slot.
        }
    }

    // Attach to the stream using the selected slot.
    tx_attachments_[slot].set_attaching(channel, sid);
    tx_current_attachment_ = &tx_attachments_[slot];

    // Fill in the new stream's USID, if it doesn't have one yet.
    if (usid_.is_empty())
    {
        set_usid(unique_stream_id_t(sid, channel->tx_channel_id()));
        logger::debug() << this << " Creating stream " << usid_;
    }

    // Get us in line to transmit on the channel.
    // We at least need to transmit an attach message of some kind;
    // in the case of Init or Reply it might also include data.

    assert(!contains(channel->sending_streams_, this));
    tx_enqueue_channel();
    if (channel->may_transmit())
        channel->on_ready_transmit();
}

void base_stream::set_usid(unique_stream_id_t new_usid)
{
    assert(usid_.is_empty());
    assert(!new_usid.is_empty());

    if (contains(peer_->usid_streams_, new_usid))
        logger::warning() << "Internal stream set_usid passed a duplicate stream USID " << new_usid;

    usid_ = new_usid;
    peer_->usid_streams_.insert(make_pair(usid_, this));
}

ssize_t base_stream::bytes_available() const
{
    return 0;
}

bool base_stream::at_end() const
{
    return true;
}

ssize_t base_stream::read_data(char* data, ssize_t max_size)
{
    ssize_t actual_size = 0;

    while (max_size > 0 and rx_available_ > 0)
    {
        assert(!end_read_);
        assert(!rx_segments_.empty());
        rx_segment_t rseg = rx_segments_.front();
        rx_segments_.pop_front();

        ssize_t size = rseg.segment_size();
        assert(size >= 0);

        // XXX BUG: this breaks if we try to read a partial segment!
        assert(max_size >= size);

        // Copy the data (or just drop it if data == nullptr).
        if (data != nullptr) {
            memcpy(data, rseg.buf.data() + rseg.header_len, size);
            data += size;
        }
        actual_size += size;
        max_size -= size;

        // Adjust the receive stats
        rx_available_ -= size;
        rx_buffer_used_ -= size;
        assert(rx_available_ >= 0);

        if (has_pending_records())
        {
            // We're reading data from a queued message.
            int64_t& headsize = rx_record_sizes_.front();
            headsize -= size;
            assert(headsize >= 0);

            // Always stop at the next message boundary.
            if (headsize == 0) {
                rx_record_sizes_.pop_front();
                break;
            }
        }
        else
        {
            // No queued messages - just read raw data.
            rx_record_available_ -= size;
            assert(rx_record_available_ >= 0);
        }

        // If this segment has the end-marker set, that's it...
        if (rseg.flags() & flags::data_close)
            shutdown(stream::shutdown_mode::read);
    }

    // Recalculate the receive window, now that we've (presumably) freed some buffer space.
    recalculate_receive_window();

    return actual_size;
}

int base_stream::pending_records() const
{
    return rx_record_sizes_.size();
}

ssize_t base_stream::pending_record_size() const
{
    return has_pending_records() ? rx_record_sizes_.front() : -1;
}

ssize_t base_stream::read_record(char* data, ssize_t max_size)
{
    if (!has_pending_records())
        return -1;  // No complete records available

    // Read as much of the next queued message as we have room for
    size_t rx_message_count_before = rx_record_sizes_.size();
    ssize_t actual_size = base_stream::read_data(data, max_size);
    assert(actual_size > 0);

    // If the message is longer than the supplied buffer, drop the rest.
    if (rx_record_sizes_.size() == rx_message_count_before)
    {
        ssize_t skip_size = base_stream::read_data(nullptr, 1 << 30);
        assert(skip_size > 0);
    }
    assert(rx_record_sizes_.size() == rx_message_count_before - 1);

    return actual_size;
}

byte_array base_stream::read_record(ssize_t max_size)
{
    ssize_t rec_size = pending_record_size();
    if (rec_size <= 0)
        return byte_array(); // No complete messages available

    // Read the next message into a new byte array
    byte_array buf;
    ssize_t buf_size = min(rec_size, max_size);
    buf.resize(buf_size);

    ssize_t actual_size = read_record(buf.data(), buf_size);
    assert(actual_size == buf_size);

    return buf;
}

ssize_t base_stream::write_data(const char* data, ssize_t total_size, uint8_t endflags)
{
    assert(!end_write_);
    ssize_t actual_size = 0;

    do {
        // Choose the size of this segment.
        ssize_t size = mtu;
        uint8_t flags = 0;

        if (total_size <= size) {
            flags = flags::data_push | endflags;
            size = total_size;
        }

        logger::debug() << "Transmit segment at " << tx_byte_seq_ << " size " << size << " bytes";

        // Build the appropriate packet header.
        packet p(this, packet_type::data);
        p.tx_byte_seq = tx_byte_seq_;

        // Prepare the header
        auto header = p.header<data_header>();
        p.buf.resize(p.buf.size() + size); // Accomodate buffer space for payload
        header = p.header<data_header>(); ///@fixme resize may have moved the buffer - do it before

        // header->sid - later
        header->type_subtype = flags;  // Major type filled in later
        // header->win - later
        // header->tsn - later

        // Advance the BSN to account for this data.
        tx_byte_seq_ += size;

        // Copy in the application payload
        char *payload = reinterpret_cast<char*>(header + 1);
        memcpy(payload, data, size);

        // Hold onto the packet data until it gets ACKed
        tx_waiting_ack_.insert(p.tx_byte_seq);
        tx_waiting_size_ += size;

        // Queue up the segment for transmission ASAP
        tx_enqueue_packet(p);

        // On to the next segment...
        data += size;
        total_size -= size;
        actual_size += size;
    } while (total_size != 0);

    if (endflags & flags::data_close)
        end_write_ = true;

    return actual_size;
}

ssize_t base_stream::read_datagram(char* data, ssize_t max_size)
{
    return 0;
}

ssize_t base_stream::write_datagram(const char* data, ssize_t size, stream::datagram_type is_reliable)
{
    return 0;
}

byte_array base_stream::read_datagram(ssize_t max_size)
{
    return byte_array();
}

void base_stream::set_priority(int priority)
{
    if (current_priority() != priority) {
        super::set_priority(priority);

        if (tx_enqueued_channel_)
        {
            stream_channel* chan = tx_current_attachment_->channel_;
            assert(chan->is_active());

            chan->dequeue_stream(this);
            chan->enqueue_stream(this);
        }
    }
}

// @todo Return unique_ptr?
abstract_stream* base_stream::open_substream()
{
    logger::debug() << "Internal stream open substream";

    // Create a new sub-stream.
    // Note that the parent doesn't have to be attached yet:
    // the substream will attach and wait for the parent if necessary.
    base_stream* new_stream = new base_stream(host_, peerid_, shared_from_this());
    new_stream->state_ = state::connected;

    // Start trying to attach the new stream, if possible.
    new_stream->attach_for_transmit();

    return new_stream;
}

abstract_stream* base_stream::accept_substream()
{
    logger::debug() << "Internal stream accept substream";
    return 0;
}

bool base_stream::is_link_up() const
{
    return state_ == state::connected;
}

void base_stream::set_receive_buffer_size(size_t size)
{
    logger::debug() << "Setting internal stream receive buffer size " << size << " bytes";
}

void base_stream::set_child_receive_buffer_size(size_t size)
{
    logger::debug() << "Setting internal stream child receive buffer size " << size << " bytes";
}

void base_stream::shutdown(stream::shutdown_mode mode)
{
    logger::debug() << "Shutting down internal stream";

    // @todo self-destruct when done, if appropriate

    // @fixme clean this flag mess up
    uint8_t fmode = to_underlying(mode);

    if (fmode & to_underlying(stream::shutdown_mode::reset))
        return disconnect();    // No graceful close necessary

    if (is_link_up() && !end_read_ && (fmode & to_underlying(stream::shutdown_mode::read)))
    {
        // Shutdown for reading
        rx_available_ = 0;
        rx_record_available_ = 0;
        rx_buffer_used_ = 0;
        readahead_.clear();
        rx_segments_.clear();
        rx_record_sizes_.clear();
        end_read_ = true;
    }

    if (is_link_up() && !end_write_ && (fmode & to_underlying(stream::shutdown_mode::write)))
    {
        // Shutdown for writing
        write_data(nullptr, 0, flags::data_close);
    }
}

void base_stream::disconnect()
{
    logger::debug() << "Disconnecting internal stream";
    state_ = state::disconnected;
    // @todo bring down the connection - clear()

    if (auto stream = owner_.lock())
    {
        stream->on_link_down();
        // @todo stream->reset()?
    }
}

void base_stream::fail(string const& error)
{
    disconnect();
    set_error(error);
    logger::warning() << error;
}

void base_stream::dump()
{
    logger::debug() << "Internal stream " << this
                    << " state " << int(state_)
                    << " TSN " << tx_byte_seq_
                    << " RSN " << rx_byte_seq_
                    << " rx_avail " << rx_available_
                    << " readahead " << readahead_.size()
                    << " rx_segs " << rx_segments_.size()
                    << " rx_rec_avail " << rx_record_available_
                    << " rx_recs " << rx_record_sizes_.size();
}

//-------------------------------------------------------------------------------------------------
// Packet transmission
//-------------------------------------------------------------------------------------------------

void base_stream::tx_enqueue_packet(packet& p)
{
    // Add the packet to our stream-local transmit queue.
    // Keep packets in order of transmit sequence number,
    // but in FIFO order for packets with the same sequence number.
    // This happens because datagram packets get assigned the current TSN
    // when they are queued, but without actually incrementing the TSN,
    // just to keep them in the right order with respect to segments.
    // (The assigned TSN is not transmitted in the datagram, of course).
    auto it = tx_queue_.begin();
    while (it != tx_queue_.end() and ((*it).tx_byte_seq - p.tx_byte_seq) <= 0)
        ++it;
    tx_queue_.insert(it, p);

    tx_enqueue_channel(/*immediately:*/true);
}

void base_stream::tx_enqueue_channel(bool tx_immediately)
{
    if (!is_attached())
        return attach_for_transmit();

    logger::debug() << "Internal stream enqueue on channel";

    stream_channel* channel = tx_current_attachment_->channel_;
    assert(channel and channel->is_active());

    if (!tx_enqueued_channel_)
    {
        if (tx_queue_.empty())
        {
            if (auto stream = owner_.lock()) {
                stream->on_ready_write();
            }
        }
        else
        {
            channel->enqueue_stream(this);
            tx_enqueued_channel_ = true;
        }
    }

    if (tx_immediately && channel->may_transmit())
        channel->got_ready_transmit();
}

void base_stream::tx_attach()
{
    logger::debug() << "Internal stream tx_attach";

    stream_channel* chan = tx_current_attachment_->channel_;
    unsigned slot = tx_current_attachment_ - tx_attachments_; // either 0 or 1
    assert(slot < max_attachments);

    // Build the Attach packet header
    packet p(this, packet_type::attach);
    auto header = p.header<attach_header>();

    header->stream_id = tx_current_attachment_->stream_id_;
    header->type_subtype = type_and_subtype(packet_type::attach,
                 (init_ ? flags::attach_init : 0) | (slot & flags::attach_slot_mask));
    header->window = receive_window_byte();

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
    p.buf.append(body);

    // Transmit it on the current channel.
    packet_seq_t pktseq;
    chan->channel_transmit(p.buf, pktseq);

    // Save the attach packet in the channel's waiting_ack_ hash,
    // so that we'll be notified when the attach packet gets acked.
    p.late = false;
    chan->waiting_ack_.insert(make_pair(pktseq, p));
}

void base_stream::tx_attach_data(packet_type type, stream_id_t ref_sid)
{
    packet p = tx_queue_.front();
    tx_queue_.pop_front();

    assert(p.type == packet_type::data);
    assert(p.tx_byte_seq <= 0xffff);

    // Build the init_header.
    auto header = as_header<init_header>(p.buf);
    header->stream_id = tx_current_attachment_->stream_id_;
    // Preserve flags already set.
    header->type_subtype = type_and_subtype(type, header->type_subtype); //@fixme & dataAllFlags);
    header->window = receive_window_byte();
    header->new_stream_id = ref_sid;
    header->tx_seq_no = p.tx_byte_seq; // Note: 16-bit TSN

    logger::debug() << p;

    // Transmit
    return tx_data(p);
}

void base_stream::tx_data(packet& p)
{
    stream_channel* channel = tx_current_attachment_->channel_;

    // Transmit the packet on our current channel.
    packet_seq_t pktseq;
    channel->channel_transmit(p.buf, pktseq);

    logger::debug() << "tx_data " << pktseq << " pos " << p.tx_byte_seq << " size " << p.buf.size();

    // Save the data packet in the channel's ackwait hash.
    p.late = false;
    channel->waiting_ack_.insert(make_pair(pktseq, p));

    // Re-queue us on our channel immediately if we still have more data to send.
    if (tx_queue_.empty())
    {
        if (auto stream = owner_.lock()) {
            stream->on_ready_write();
        }
    } else {
        tx_enqueue_channel();
    }
}

void base_stream::tx_datagram()
{
    logger::debug() << this << " base_stream::tx_datagram";

    // Transmit the whole datagram immediately,
    // so that all fragments get consecutive packet sequence numbers.
    while (true)
    {
        assert(!tx_queue_.empty());
        packet p = tx_queue_.front();
        tx_queue_.pop_front();
        assert(p.type == packet_type::datagram);

        auto header = as_header<datagram_header>(p.buf);
        bool at_end = (header->type_subtype & flags::datagram_end) != 0;
        header->stream_id = tx_current_attachment_->stream_id_;
        header->window = receive_window_byte();

        // Adjust the in-flight byte count.
        // We don't need to register datagram packets in tx_inflight_
        // because we don't keep them around after they're "missed" -
        // which is fortunate since we _can't_ register them
        // because they don't have unique TSNs.
        tx_inflight_ += p.payload_size();

        // Transmit this datagram packet, but don't save it anywhere - just fire & forget.
        packet_seq_t pktseq;
        tx_current_attachment_->channel_->channel_transmit(p.buf, pktseq);

        if (at_end)
            break;
    }

    // Re-queue us on our flow immediately if we still have more data to send.
    return tx_enqueue_channel();
}

// @todo Complete the code.
void base_stream::tx_reset(stream_channel* channel, stream_id_t sid, uint8_t flags)
{
    logger::warning() << "base_stream::tx_reset UNIMPLEMENTED";

    // Build the Reset packet header
    packet p(nullptr, packet_type::reset);
    auto header = p.header<reset_header>();

    header->stream_id = sid;
    header->type_subtype = type_and_subtype(packet_type::reset, flags);
    header->window = 0;

    // Transmit it on the current channel.
    packet_seq_t pktseq;
    channel->channel_transmit(p.buf, pktseq);

    // Save the attach packet in the channel's waiting_ack_ hash,
    // so that we'll be notified when the attach packet gets acked.
    // XXX for the packets with O flag set, we don't need to ack??
    if (!(flags & flags::reset_remote_sid))
    {
        p.late = false;
        channel->waiting_ack_.insert(make_pair(pktseq, p));
    }

    logger::debug() << channel << " Reset packet sent, XXX garbage collect the stream!";

    // abort the stream
    // send RESET packet to the peer

// as per the PDF:
// As in TCP, either host may unilaterally terminate an SST stream in both directions and discard 
// any buffered data. A host resets a stream by sending a Reset packet (Figure 6) containing 
// an LSID in either the sender’s or receiver’s LSID space, and an O (Orientation) flag indicating
// in which space the LSID is to be interpreted. When a host uses a Reset packet to terminate 
// a stream it believes to be active, it uses its own LSID referring to the stream, and resends
// the Reset packet as necessary until it obtains an acknowledgment. A host also sends a Reset
// in response to a packet it receives referring to an unknown LSID or USID. This situation 
// may occur if the host has closed and garbage collected its state for a stream but one of its
// acknowledgments to its peer’s data segments is lost in transit, causing its peer to retransmit
// those segments. The stateless Reset response indicates to the peer that it can garbage collect 
// its stream state as well. Stateless Reset responses always refer to the peer’s LSID space, 
// since by definition the host itself does not have an LSID assigned to the unknown stream.
}

void base_stream::acknowledged(stream_channel* channel, packet const& pkt, packet_seq_t rx_seq)
{
    logger::debug() << this << " ACKed packet of size " << pkt.buf.size();

    switch (pkt.type)
    {
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

                logger::debug() << "tx_waiting_ack remove " << pkt.tx_byte_seq << " size " << pkt.payload_size()
                         << " new wait count " << tx_waiting_ack_.size() << " waiting to ack " << tx_waiting_size_
                         << " bytes";
            }
            assert(tx_waiting_size_ >= 0);
            if (auto stream = owner_.lock())
                stream->on_bytes_written(pkt.payload_size()); // XXX delay and coalesce signal

            // fall through...

        case packet_type::attach:
            if (tx_current_attachment_ 
                and tx_current_attachment_->channel_ == channel
                and !tx_current_attachment_->is_acknowledged())
            {
                // We've gotten our first ack for a new attachment.
                // Save the rxseq the ack came in on as the attachment's reference pktseq.
                logger::debug() << this << " Got attach ack " << rx_seq;
                tx_current_attachment_->set_active(rx_seq);
                init_ = false;

                // Normal data transmission may now proceed.
                tx_enqueue_channel();

                // Notify anyone interested that we're attached.
                on_attached();
                auto stream = owner_.lock();
                if (stream and state_ == state::connected)
                    stream->on_link_up();
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
            logger::warning() << "Got ACK for unknown packet type " << int(pkt.type);
            break;
    }
}

bool base_stream::missed(stream_channel* channel, packet const& pkt)
{
    assert(pkt.late);

    logger::debug() << this << " Missed seq " << pkt.tx_byte_seq << " of size " << pkt.buf.size();

    switch (pkt.type)
    {
        case packet_type::data: {
            logger::debug() << this << " Retransmit seq " << pkt.tx_byte_seq << " of size " << pkt.payload_size();
            // Mark the segment no longer "in flight".
            end_flight(pkt);
            // Retransmit reliable segments...
            packet p = pkt;
            tx_enqueue_packet(p); 
            return true; // ...but keep the tx record until expiry in case it gets acked late!
        }

        case packet_type::attach:
            logger::debug() << this << " Attach packet lost: trying again to attach";
            tx_enqueue_channel();
            return true;

        case packet_type::datagram:
            logger::debug() << this << "Datagram packet lost: oops, gone for good";

            // Mark the segment no longer "in flight".
            // We know we'll only do this once per DatagramPacket
            // because we drop it immediately below ("return false");
            // thus acked() cannot be called on it after this.
            tx_inflight_ -= pkt.payload_size();  // no longer "in flight"
            assert(tx_inflight_ >= 0);

            return false;

        case packet_type::ack:
        case packet_type::detach:
        case packet_type::reset:
        default:
            logger::warning() << "Missed unknown packet type " << int(pkt.type);
            return false;
    }
}

void base_stream::expire(stream_channel* channel, packet const& pkt)
{
    // do nothing for now
    // @fixme
}

// Cancel its allocation in our or our parent stream's state,
// according to the type of packet actually sent.
void base_stream::end_flight(packet const& pkt)
{
    auto header = as_header<data_header>(pkt.buf);

    if (type_from_header(header) == packet_type::init)
    {
        if (auto parent = parent_.lock())
        {
            parent->tx_inflight_ -= pkt.payload_size();
            logger::debug() << this << " Endflight " << pkt.tx_byte_seq
                << ", bytes in flight on parent " << parent->tx_inflight_;
            assert(parent->tx_inflight_ >= 0);
        }
    } else {    // Reply or Data packet
        tx_inflight_ -= pkt.payload_size();
        logger::debug() << this << " Endflight " << pkt.tx_byte_seq
            << ", bytes in flight " << tx_inflight_;
        assert(tx_inflight_ >= 0);
    }
}

//-------------------------------------------------------------------------------------------------
// Packet reception
//-------------------------------------------------------------------------------------------------

constexpr size_t header_len_min          = channel::header_len + 4;
constexpr size_t init_header_len_min     = channel::header_len + 8;
constexpr size_t reply_header_len_min    = channel::header_len + 8;
constexpr size_t data_header_len_min     = channel::header_len + 8;
constexpr size_t datagram_header_len_min = channel::header_len + 4;
constexpr size_t ack_header_len_min      = channel::header_len + 4;
constexpr size_t reset_header_len_min    = channel::header_len + 4;
constexpr size_t attach_header_len_min   = channel::header_len + 4;
constexpr size_t detach_header_len_min   = channel::header_len + 4;

// Main packet receive entry point, called from stream_channel::channel_receive()
bool base_stream::receive(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < header_len_min) {
        logger::warning() << "Received runt packet";
        return false; // @fixme Protocol error, close channel?
    }

    auto header = as_header<stream_header>(pkt);

    switch (type_from_header(header))
    {
        case packet_type::init:
            return rx_init_packet(pktseq, pkt, channel);
        case packet_type::reply:
            return rx_reply_packet(pktseq, pkt, channel);
        case packet_type::data:
            return rx_data_packet(pktseq, pkt, channel);
        case packet_type::datagram:
            return rx_datagram_packet(pktseq, pkt, channel);
        case packet_type::ack:
            return rx_ack_packet(pktseq, pkt, channel);
        case packet_type::reset:
            return rx_reset_packet(pktseq, pkt, channel);
        case packet_type::attach:
            return rx_attach_packet(pktseq, pkt, channel);
        case packet_type::detach:
            return rx_detach_packet(pktseq, pkt, channel);
        default:
            logger::warning() << "Unknown packet type " << hex << int(type_from_header(header));
            return false; // @fixme Protocol error, close channel?
    }
}

bool base_stream::rx_init_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < init_header_len_min) {
        logger::warning() << "Received runt init packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::debug() << "rx_init_packet ...";
    auto header = as_header<init_header>(pkt);

    // Look up the stream - if it already exists,
    // just dispatch it directly as if it were a data packet.
    if (contains(channel->receive_sids_, header->stream_id))
    {
        logger::debug() << "rx_init_packet: stream exists, dispatch data only";
        stream_attachment* attach = channel->receive_sids_[header->stream_id];
        if (pktseq < attach->sid_seq_) // earlier init packet; that's OK.
            attach->sid_seq_ = pktseq;

        channel->ack_sid_ = header->stream_id;
        attach->stream_->recalculate_transmit_window(header->window);
        attach->stream_->rx_data(pkt, header->tx_seq_no);
        return true; // ACK the packet
    }

    // Doesn't yet exist - look up the parent stream.
    if (!contains(channel->receive_sids_, header->new_stream_id))
    {
        // The parent SID is in error, so reset that SID.
        // Ack the pktseq first so peer won't ignore the reset!
        logger::warning() << "rx_init_packet: unknown parent stream ID";
        channel->acknowledge(pktseq, false);
        tx_reset(channel, header->new_stream_id, flags::reset_remote_sid);
        return false;
    }

    stream_attachment* parent_attach = channel->receive_sids_[header->new_stream_id];
    logger::debug() << "rx_init_packet: found parent stream attach " << parent_attach;
    if (pktseq < parent_attach->sid_seq_)
    {
        logger::warning() << "rx_init_packet: stale wrt parent SID sequence";
        return false; // silently drop stale packet
    }

    // Extrapolate the sender's stream counter from the new SID it sent,
    // and use it to form the new stream's USID.
    counter_t ctr = channel->received_sid_counter_ +
        (int16_t)(header->stream_id - (int16_t)channel->received_sid_counter_);
    unique_stream_id_t usid(ctr, channel->rx_channel_id());

    logger::debug() << "rx_init_packet: parent attach stream " << parent_attach->stream_;

    // Create the new substream.
    base_stream* new_stream = parent_attach->stream_->rx_substream(pktseq, channel, header->stream_id, 0, usid);
    if (!new_stream)
        return false;

    // Now process any data segment contained in this init packet.
    channel->ack_sid_ = header->stream_id;
    new_stream->recalculate_transmit_window(header->window);
    new_stream->rx_data(pkt, header->tx_seq_no);

    return false; // Already acknowledged in rx_substream().
}

bool base_stream::rx_reply_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < reply_header_len_min) {
        logger::warning() << "Received runt reply packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::debug() << "rx_reply_packet ...";
    auto header = as_header<reply_header>(pkt);

    // Look up the stream - if it already exists,
    // just dispatch it directly as if it were a data packet.
    if (contains(channel->receive_sids_, header->stream_id))
    {
        logger::debug() << "rx_reply_packet: stream exists, dispatch data only";
        stream_attachment* attach = channel->receive_sids_[header->stream_id];
        if (pktseq < attach->sid_seq_) // earlier reply packet; that's OK.
            attach->sid_seq_ = pktseq;

        channel->ack_sid_ = header->stream_id;
        attach->stream_->recalculate_transmit_window(header->window);
        attach->stream_->rx_data(pkt, header->tx_seq_no);
        return true; // ACK the packet
    }

    // Doesn't yet exist - look up the reference stream in our SID space.
    if (!contains(channel->transmit_sids_, header->new_stream_id))
    {
        // The reference SID supposedly in our own space is invalid!
        // Respond with a reset indicating that SID in our space.
        // Ack the pktseq first so peer won't ignore the reset!
        logger::debug() << "rx_reply_packet: unknown reference stream ID";
        channel->acknowledge(pktseq, false);
        tx_reset(channel, header->new_stream_id, 0);
        return false;
    }

    stream_attachment* attach = channel->transmit_sids_[header->new_stream_id];

    if (pktseq < attach->sid_seq_)
    {
        logger::debug() << "rx_reply_packet: stale packet - pktseq " << pktseq
            << " sidseq " << attach->sid_seq_;
        return false;   // silently drop stale packet
    }

    base_stream* stream = attach->stream_;

    logger::debug() << stream << " Accepting reply " << stream->usid_;

    // OK, we have the stream - just create the receive-side attachment.
    stream->rx_attachments_[0].set_active(channel, header->stream_id, pktseq);

    // Now process any data segment contained in this reply packet.
    channel->ack_sid_ = header->stream_id;
    stream->recalculate_transmit_window(header->window);
    stream->rx_data(pkt, header->tx_seq_no);

    return true;    // Acknowledge the packet
}

bool base_stream::rx_data_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < data_header_len_min) {
        logger::warning() << "Received runt data packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::warning() << "rx_data_packet ...";
    auto header = as_header<data_header>(pkt);

    if (!contains(channel->receive_sids_, header->stream_id))
    {
        // Respond with a reset for the unknown stream ID.
        // Ack the pktseq first so peer won't ignore the reset!
        logger::debug() << "rx_data_packet: unknown stream ID " << header->stream_id;
        channel->acknowledge(pktseq, false);
        tx_reset(channel, header->stream_id, flags::reset_remote_sid);
        return false;
    }

    stream_attachment* attach = channel->receive_sids_[header->stream_id];
    if (pktseq < attach->sid_seq_)
    {
        logger::debug() << "rx_data_packet: stale packet - pktseq " << pktseq
            << " sidseq " << attach->sid_seq_;
        return false;   // silently drop stale packet
    }

    // Process the data, using the full 32-bit TSN.
    channel->ack_sid_ = header->stream_id;
    attach->stream_->recalculate_transmit_window(header->window);
    attach->stream_->rx_data(pkt, header->tx_seq_no);
    return true; // ACK the packet
}

bool base_stream::rx_datagram_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < datagram_header_len_min) {
        logger::warning() << "Received runt datagram packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::warning() << "rx_datagram_packet UNIMPLEMENTED.";
    // auto header = as_header<datagram_header>(pkt);
    return false;
}

bool base_stream::rx_ack_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < ack_header_len_min) {
        logger::warning() << "Received runt ack packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::warning() << "rx_ack_packet UNIMPLEMENTED.";
    // auto header = as_header<ack_header>(pkt);
    return false;
}

/**
 * @todo Received a reset packet, forcefully reset stream.
 * @param  pktseq  Packet sequence number.
 * @param  pkt     Reset packet itself.
 * @param  channel Associated channel.
 * @return         true if reset is successful.
 */
bool base_stream::rx_reset_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < reset_header_len_min) {
        logger::warning() << "Received runt reset packet";
        return false; // @fixme Protocol error, close channel?
    }

    logger::warning() << "rx_reset_packet UNIMPLEMENTED.";
    // auto header = as_header<reset_header>(pkt);
    // bool local_sid = hdr->type & flags::reset_remote_sid;
    return false;
}

bool base_stream::rx_attach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < attach_header_len_min) {
        logger::warning() << "Received runt attach packet";
        return false; // @fixme Protocol error, close channel?
    }

    auto header = as_header<attach_header>(pkt);
    bool init = (header->type_subtype & flags::attach_init) != 0;
    int slot = header->type_subtype & flags::attach_slot_mask;
    unique_stream_id_t usid, parent_usid;

    logger::debug() << "Received attach packet, " << (init ? "init" : "non-init") << " attach on slot " << slot;

    byte_array_iwrap<flurry::iarchive> read(pkt);
    read.archive().skip_raw_data(sizeof(attach_header) + channel::header_len);

    // Decode the USID(s) in the body
    read.archive() >> usid;
    if (init) {
        read.archive() >> parent_usid;
    }

    if (usid.is_empty() or (init and parent_usid.is_empty()))
    {
        logger::warning() << "Invalid attach packet received";
        return false; // @fixme Protocol error, shutdown channel?
    }

    if (contains(channel->peer_->usid_streams_, usid))
    {
        // Found it: the stream already exists, just attach it.
        base_stream* stream = channel->peer_->usid_streams_[usid];

        logger::debug() << "Found USID in existing streams";
        channel->ack_sid_ = header->stream_id;
        stream_rx_attachment& rslot = stream->rx_attachments_[slot];
        if (rslot.is_active())
        {
            if (rslot.channel_ == channel and rslot.stream_id_ == header->stream_id)
            {
                logger::debug() << stream << " redundant attach " << stream->usid_;
                rslot.sid_seq_ = min(rslot.sid_seq_, pktseq);
                return true;
            }
            logger::debug() << stream << " replacing attach slot " << slot;
            rslot.clear();
        }
        logger::debug() << stream << " accepting attach " << stream->usid_;
        rslot.set_active(channel, header->stream_id, pktseq);
        return true;
    }

    // Stream doesn't exist - lookup parent if it's an init-attach.
    base_stream* parent_stream{nullptr};

    for (auto x : channel->peer_->usid_streams_) {
        logger::debug() << "known usid " << x.first;
    }

    if (init && contains(channel->peer_->usid_streams_, parent_usid)) {
        parent_stream = channel->peer_->usid_streams_[parent_usid];
    }
    if (parent_stream != NULL)
    {
        // Found it: create and attach a child stream.
        channel->ack_sid_ = header->stream_id;
        parent_stream->rx_substream(pktseq, channel, header->stream_id, slot, usid);
        return false;   // Already acked in rx_substream()
    }

    // No way to attach the stream - just reset it.
    logger::debug() << "rx_attach_packet: unknown stream " << usid;
    channel->acknowledge(pktseq, false);
    tx_reset(channel, header->stream_id, flags::reset_remote_sid);
    return false;
}

/**
 * @todo Received a detach packet, disconnect stream from the channel.
 */
bool base_stream::rx_detach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel)
{
    if (pkt.size() < detach_header_len_min) {
        logger::warning() << "Received runt detach packet";
        return false; // @fixme Protocol error, close channel?
    }

    // auto header = as_header<detach_header>(pkt);
    // @todo
    logger::fatal() << "rx_detach_packet UNIMPLEMENTED.";
    return false;
}

void base_stream::rx_data(byte_array const& pkt, uint32_t byte_seq)
{
    if (end_read_) {
        // Ignore anything we receive past end of stream
        // (which we may have forced from our end via close()).
        logger::warning() << this << " Ignoring segment received after end-of-stream";
        assert(readahead_.empty());
        assert(rx_segments_.empty());
        return;
    }

    rx_segment_t rseg(pkt, byte_seq, channel::header_len + sizeof(data_header));
    int seg_size = rseg.segment_size();

    logger::warning() << "rx_data " << byte_seq << " payload size " << seg_size;

    // See where this packet fits in
    int rx_seq_diff = rseg.rx_byte_seq - rx_byte_seq_;
    if (rx_seq_diff <= 0) {
        // The segment is at or before our current receive position.
        // How much of its data, if any, is actually useful?
        // Note that we must process packets at our RSN with no data,
        // because they might have important flags.
        int act_size = seg_size + rx_seq_diff;

        // It gives us exactly the data we want next - very good!
        bool wasnomsgs = !has_pending_records();
        bool closed = false;

        rx_enqueue_segment(rseg, act_size, /*inout*/closed);

        if (wasnomsgs && has_pending_records()) {
            if (state_ == state::connected) {
            } else if (state_ == state::wait_service) {
                got_service_reply();
            } else if (state_ == state::accepting) {
                got_service_request();
            }
        }
    }

    // Recalculate the receive window now that we've probably consumed some buffer space.
    recalculate_receive_window();
}

base_stream* base_stream::rx_substream(packet_seq_t pktseq, stream_channel* channel,
            stream_id_t sid, unsigned slot, unique_stream_id_t const& usid)
{
    // Make sure we're allowed to create a child stream.
    if (!is_listening()) {
        // The parent SID is not in error, so just reset the new child.
        // Ack the pktseq first so peer won't ignore the reset!
        logger::debug() << "Other side trying to create substream, but we're not listening.";
        channel->acknowledge(pktseq, false);
        tx_reset(channel, sid, flags::reset_remote_sid);
        return nullptr;
    }

    // Mark the Init packet received now, before transmitting the Reply;
    // otherwise the Reply won't acknowledge the Init
    // and the peer will reject it as a stale packet.
    channel->acknowledge(pktseq, true);

    // Create the child stream.
    base_stream* new_stream = new base_stream(channel->get_host(), peerid_, shared_from_this());

    // We'll accept the new stream: this is the point of no return.
    logger::debug() << "Accepting sub-stream " << usid << " as " << new_stream;

    // Extrapolate the sender's stream counter from the new SID it sent.
    counter_t ctr = channel->received_sid_counter_ +
        (int16_t)(sid - (int16_t)channel->received_sid_counter_);
    if (ctr > channel->received_sid_counter_)
        channel->received_sid_counter_ = ctr;

    // Use this stream counter to form the new stream's USID.
    // @fixme ctr is not used here??
    new_stream->set_usid(usid);

    // Automatically attach the child via its appropriate receive-slot.
    new_stream->rx_attachments_[slot].set_active(channel, sid, pktseq);

    // If this is a new top-level application stream,
    // we expect a service request before application data.
    if (shared_from_this() == channel->root_)
    {
        new_stream->state_ = state::accepting; // Service request expected
    }
    else
    {
        new_stream->state_ = state::connected;
        received_substreams_.push(new_stream);
        new_stream->on_ready_read_message.connect(
            boost::bind(&base_stream::substream_read_message, this));
        if (auto stream = owner_.lock())
            stream->on_new_substream();
    }

    return new_stream;
}

// Helper function to enqueue useful data segments.
void base_stream::rx_enqueue_segment(rx_segment_t const& seg, size_t actual_size, bool& closed)
{
    rx_segments_.push_back(seg);
    rx_byte_seq_ += actual_size;
    rx_available_ += actual_size;
    rx_record_available_ += actual_size;
    rx_buffer_used_ += actual_size;

    if ((seg.flags() & (flags::data_message | flags::data_close)) and (rx_record_available_ > 0))
    {
        logger::debug() << "Received record";
        rx_record_sizes_.push_back(rx_record_available_);
        rx_record_available_ = 0;
    }
    if (seg.flags() & flags::data_close)
        closed = true;
}

static inline byte_array service_reply(stream_protocol::service_code reply, string message)
{
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << stream_protocol::service_code::connect_reply << reply << message;
    }
    return msg;
}

void base_stream::got_service_request()
{
    assert(state_ == state::accepting);

    byte_array_iwrap<flurry::iarchive> read(read_record(max_service_record_size));
    uint32_t code;
    string service, protocol;
    read.archive() >> code >> service >> protocol; // @fixme may throw..
    if (code != service_code::connect_request)
        return fail("Bad service request");

    logger::debug() << "got_service_request service '" << service << "' protocol '" << protocol << "'";

    // Lookup the requested service
    server* srv = host_->listener_for(service, protocol);

    if (!srv)
    {
        ostringstream oss;
        oss << "Request for service " << service << " with unknown protocol " << protocol;
        write_record(service_reply(service_code::reply_not_found, oss.str()));
        return fail(oss.str());
    }

    // Send a service reply to the client
    write_record(service_reply(service_code::reply_ok, "ok"));

    // Hand off the new stream to the chosen service
    state_ = state::connected;
    srv->received_connections_.push(this);
    srv->on_new_connection();
}

void base_stream::got_service_reply()
{
    assert(state_ == state::wait_service);
    assert(tx_current_attachment_);

    logger::debug() << "got_service_reply";

    byte_array_iwrap<flurry::iarchive> read(read_record(max_service_record_size));
    uint32_t code, status;
    string message;
    read.archive() >> code >> status >> message;
    if (code != service_code::connect_reply or status != 0)
    {
        ostringstream oss;
        oss << "Service connect failed with code " << code << " status " << status
            << " message " << message;
        return fail(oss.str());
    }

    state_ = state::connected;
    if (auto stream = owner_.lock())
        stream->on_link_up();
}

//-----------------
// Signal handlers
//-----------------

void base_stream::channel_connected()
{
    logger::debug() << "Internal stream - channel has connected.";
    if (peer_)
    {
        peer_->on_channel_connected.disconnect(boost::bind(&base_stream::channel_connected, this));
    }

    // Retry attach now that we hopefully have an active channel.
    attach_for_transmit();
}

void base_stream::parent_attached()
{
    logger::debug() << "Internal stream - parent stream has attached, we can now attach.";
    if (auto parent = parent_.lock())
    {
        parent->on_attached.disconnect(boost::bind(&base_stream::parent_attached, this));
    }

    // Retry attach now that parent hopefully has a USID.
    attach_for_transmit();
}

void base_stream::substream_read_message()
{
    // When one of our queued subs emits an on_ready_read_message() signal,
    // we have to forward that via our on_ready_read_datagram() signal.
    // @fixme WHY?
    if (auto stream = owner_.lock())
        stream->on_ready_read_datagram();
}

//=================================================================================================
// stream_tx_attachment
// Where our stream attaches to channel.
//=================================================================================================

void stream_tx_attachment::set_attaching(stream_channel* channel, stream_id_t sid)
{
    assert(!is_in_use());

    logger::debug() << "Stream transmit attachment going active on channel " << channel;

    channel_ = channel;
    stream_id_ = sid;
    sid_seq_ = ~0; //@fixme magic number
    active_ = deprecated_ = false;

    assert(!contains(channel_->transmit_sids_, stream_id_));
    channel_->transmit_sids_.insert(make_pair(stream_id_, this));
}

void stream_tx_attachment::clear()
{
    stream_channel* channel = channel_;
    if (!channel)
        return;

    if (stream_->tx_current_attachment_ == this)
        stream_->tx_current_attachment_ = nullptr;   // @fixme Send notification?

    assert(contains(channel->transmit_sids_, stream_id_));
    assert(channel->transmit_sids_[stream_id_] == this);

    channel->transmit_sids_.erase(stream_id_);
    channel_ = nullptr;
    active_ = false;

    // Remove the stream from the channel's waiting streams list
    channel->dequeue_stream(stream_);
    stream_->tx_enqueued_channel_ = false;

    // Clear out packets for this stream from channel's ackwait table
    for (auto ackw : channel->waiting_ack_)
    {
        base_stream::packet& p = ackw.second;
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
        channel->waiting_ack_.erase(ackw.first); // @fixme Invalidates iterators?
    }
}

//=================================================================================================
// stream_rx_attachment
// Where the peer's stream is attached to the channel.
//=================================================================================================

void stream_rx_attachment::set_active(stream_channel* channel, stream_id_t sid, packet_seq_t rxseq)
{
    assert(!is_active());

    logger::debug() << "Stream receive attachment going active on channel " << channel;

    channel_ = channel;
    stream_id_ = sid;
    sid_seq_ = rxseq;

    assert(!contains(channel_->receive_sids_, stream_id_));
    channel_->receive_sids_.insert(make_pair(stream_id_, this));
}

void stream_rx_attachment::clear()
{
    logger::debug() << "Stream receive attachment going inactive";
    if (channel_)
    {
        assert(contains(channel_->receive_sids_, stream_id_));
        assert(channel_->receive_sids_[stream_id_] == this);
        channel_->receive_sids_.erase(stream_id_);
        channel_ = nullptr;
    }
}

} // ssu namespace
