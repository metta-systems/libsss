//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "abstract_stream.h"

namespace ssu {

class base_stream;
class stream_channel;

/**
 * Helper representing an attachment point on a stream where the stream attaches to a channel.
 */
class stream_attachment : public stream_protocol
{
protected:
    base_stream*     stream_;       ///< Our stream.
    stream_channel*  channel_{0};   ///< Channel our stream is attached to.
    id_t             stream_id_;    ///< Our stream ID in this channel.
    uint64_t         sid_seq_;      ///< Reference packet sequence for stream ID.
};

class stream_tx_attachment : public stream_attachment
{
    bool active_{false};     ///< Currently active and usable.
    bool deprecated_{false}; ///< Opening a replacement channel.

public:
    inline bool is_in_use()       const { return channel_ != nullptr; }
    inline bool is_acknowledged() const { return sid_seq_ != -1; }// todo fixme magic value
    inline bool is_active()       const { return active_; }
    inline bool is_deprecated()   const { return deprecated_; }

    /**
     * Transition from Unused to Attaching -
     * this happens when we send a first Init, Reply, or Attach packet.
     */
    void set_attaching(stream_channel* channel, id_t sid);

    /**
     * Transition from Attaching to Active -
     * this happens when we get an Ack to our Init, Reply, or Attach.
     */
    inline void set_active(uint64_t rxseq) {
        assert(is_in_use() && !is_acknowledged());
        sid_seq_ = rxseq;
        active_ = true;
    }

    // Transition to the unused state.
    void clear();
};

class stream_rx_attachment : public stream_attachment
{
public:
    inline bool is_active() const { return channel_ != nullptr; }

    // Transition from unused to active.
    void set_active(stream_channel* channel, id_t sid, uint64_t rxseq);

    // Transition to the unused state.
    void clear();
};

/**
 * @internal
 * Basic internal implementation of the abstract stream.
 * The separation between the internal stream control object and the
 * application-visible stream object is primarily needed so that SSU can
 * hold onto a stream's state and gracefully shut it down after the
 * application deletes its stream object representing it.
 * This separation also keeps the internal stream control variables out of the
 * public C++ API header files and thus able to change without breaking binary
 * compatibility, and makes it easy to implement service/protocol negotiation
 * for top-level application streams by extending this class.
 */
class base_stream : public abstract_stream
{
    enum class state {
        created = 0,   ///< Newly created.
        wait_service,  ///< Initiating, waiting for service reply.
        accepting,     ///< Accepting, waiting for service request.
        connected,     ///< Connection established.
        disconnected   ///< Connection terminated.
    };

    /**
     * @internal
     * Unit of data transmission on SSU stream.
     */
    struct packet
    {
        base_stream* owner{nullptr};
        uint64_t tsn{0};     ///< Logical byte position. XXX tx_byte_pos
        byte_array buf;      ///< Packet buffer including headers.
        int header_len{0};       ///< Size of channel and stream headers.
        packet_type type{packet_type::invalid}; ///< Type of this packet.
        bool late{false};                       ///< Possibly lost packet.

        inline packet() = default;
        inline packet(base_stream* o, packet_type t)
            : owner(o)
            , type(t)
        {}
        inline bool is_null() const {
            return owner == nullptr;
        }
        inline int payload_size() const {
            return buf.size() - header_len;
        }
    };

    std::weak_ptr<base_stream> parent; ///< Parent, if it still exists.

    static const size_t default_rx_buffer_size = 65536;

    void recalculate_receive_window();
    void recalculate_transmit_window();

    void tx_enqueue_channel(bool tx_immediately);

    bool attached();

    //====================================
    // Transmit various types of packets.
    //====================================

    void tx_attach();

public:
    /**
     * Create a base_stream instance.
     * @param host parent host
     * @param peer the endpoint identifier (EID) of the remote host with which this stream
     *        will be used to communicate. The destination may be either a cryptographic EID
     *        or a non-cryptographic legacy address as defined by the Ident class.
     * @param parent the parent stream, or NULL if none (yet).
     */
    base_stream(std::shared_ptr<host>& h, const peer_id& peer, std::shared_ptr<base_stream> parent);
    virtual ~base_stream();
};

} // namespace ssu
