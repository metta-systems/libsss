Structured Secure Streams
=========================

## 1 Introduction

SSS builds on SST, SPDY, QUIC and CurveCP protocols.

SSS provides the following set of features:
 * Multiplex many application streams onto one secure network connection
 * Streams with hereditary structure: applications can spawn lightweight streams from existing ones
   * Efficient: no 3-way handshake on startup or TIME-WAIT on close
   * Supports request/response transactions without serializing onto one stream
   * General out-of-band signaling: control requests already in progress
 * Both reliable and best-effort delivery in a semantically unified model
   * supports messages/datagrams of any size: no need to limit size of video frames, RPC responses, etc.
 * Dynamic prioritization of application's streams
   * e.g., load visible parts of a web page first, change priorities when user scrolls
 * End-to-end cryptographic security

SPDY and QUIC extend with packet framing, encoding and a set of goals to achieve (see below).

CurveCP adds opaque cryptoboxes for all crucial contents and a session initiation protocol.

### 1.1 Goals

We’d like to develop a transport that supports the following goals:
 * Widespread deployability in today’s internet (i.e., makes it through middle-boxes; runs on common user client machines without kernel changes, or elevated privileges)
 * Reduced head-of-line blocking due to packet loss (losing one packet will not generally impair other multiplexed streams)
 * Low latency (minimal round-trip costs, both during setup/resumption, and in response to packet loss)
   * Significantly reduced connection startup latency
   * Attempt to use Forward Error Correcting (FEC) codes to reduce retransmission latency after packet loss.
 * Improved support for mobile, in terms of latency and efficiency (as opposed to TCP connections which are torn down during radio shutdowns)
 * Congestion avoidance support comparable to, and friendly to, TCP (unified across multiplexed streams)
   * Individual stream flow control, to prevent a stream with a fast source and slow sink from flooding memory at receiver end, and allow back-pressure to appear at the send end.
 * Privacy assurances comparable to TLS (without requiring in-order transport or in-order decryption)
 * Reliable and safe resource requirements scaling, both server-side and client-side (including reasonable buffer management and aids to avoid facilitating DoS amplification attacks)
 * Reduced bandwidth consumption and increased channel status responsiveness (via unified signaling of channel status across all multiplexed streams)
 * Reduced packet-count, if not in conflict with other goals.
 * Support reliable transport for multiplexed streams (can simulate TCP on the multiplexed streams)
 * Efficient demux-mux properties for proxies, if not in conflict with other goals.
 * Reuse, or evolve, existing protocols at any point where it is plausible to do so, without sacrificing our stated goals (e.g., consider LEDBAT, DCCP, TCP minion)

### 1.2 Why not use SCTP over DTLS?

One recurring question is: Why not use [SCTP (Stream Control Transmission Protocol)](http://en.wikipedia.org/wiki/Stream_Control_Transmission_Protocol) over [DTLS (Datagram Transport Layer Security)](http://en.wikipedia.org/wiki/Datagram_Transport_Layer_Security)? SCTP provides (among other things) stream multiplexing, and DTLS provides SSL quality encryption and authentication over a UDP stream. In addition to the fact that both of these protocols are in various levels of standardization, this combination is currently on a standard track, and [described in this Network Working Group Internet Draft](http://tools.ietf.org/html/draft-tuexen-tsvwg-sctp-dtls-encaps-00).
The largest visible issue with using these protocols relates to our goals in the area of connection latency, and is perhaps the most critical conflicting element. In addition, we can also anticipate issues in bandwidth efficiency that may reduce our ability to achieve the goals.

### 1.3 Packet loss

Packet loss in the Internet is broadly estimated to be in the range of 1-2% of all packets. These numbers have been confirmed by tests of clients, such as Chrome, recording stats for test streams of UDP packets to server farms around the world. It is extremely unlikely that over 100 packets can be received without a loss, and certainly not 200.

The primary cause for packet loss is believed to be congestion, where routers perform switching operations, and output buffer sizes are exceeded. This issue is fundamental to the design of the Internet, and TCP, where packet loss is used as a signal of congestion, and the protocol is required to respond by reducing the flow across the congested path. Packet loss can also be caused by analog factors on transmission lines, but such losses are believed to be much lower in rate, and hence negligible.

One problem (at least with normal implementations) is that the application cannot access the packets coming after a lost packet until the retransmitted copy of the lost packet is received. This causes problems for real-time applications such as streaming media, real-time multiplayer games and voice over IP (VoIP) where it is generally more useful to get most of the data in a timely fashion than it is to get all of the data in order.

A packet is routinely delayed when a packet is lost, such as due to congestion, and it must be retransmitted. A better multiplexed transport should delay only one stream when a single packet is lost.

Packet loss will be handled by two mechanisms: Packet-level error correcting codes, and lost data retransmission. The ultimate fallback when all else fails will be retransmission of lost data. When data is retransmitted as a response to a lost packet, the original packet is not retransmitted. Instead, the encapsulated data is placed in a new packet, and that new packet is sent.

For reduced latency of shorter streams, and for the tail-end of some (all?) streams, redundant information may be added by the protocol to facilitate error correction, and reduce the need for retransmission. For larger streams, where serialization latency is deemed to be the dominant factor (by the application that constructs and send the stream), the use of FEC redundancy may be reduced or eliminated for most packets of the stream.

## 2 Design Overview

Figure 1: Protocol Architecture ([advanced diagram](https://dl.dropboxusercontent.com/s/6ck76plbu7lrun3/2014-10-02%20at%2012.21.png?dl=0))
```
  +------------------------------------------------------------------------+
  |                      Application Protocol                              |
  +------------------------------------------------------------------------+
                    + ^                                     + ^
  ------------------| | Streams-----------------------------| |---------------------------
                    v +                                     | |
  +----------------------------------------------+          | |
  |            Stream Protocol                   |          | |                 Structured
  +----------------------------------------------+          | |                 Secure
       + ^                       + ^                        | |                 Streams
       | | Frames                | |                        | |
       v +                       | |                        | |
  +------------------+           | |                        | |
  |     Framing      |           | |                        | |
  +------------------+           | |                        | |
       + ^                       | |                        | |
       | | Channels              | |                        | |
       v +                       v +                        v +
  +-------------------+    +---------------------+    +---------------------+
  |      Channel      |+-->|    Negotiation      |+-->|   Registration      |
  |      Protocol     |<--+|    Protocol         |<--+|   Protocol          |
  +-------------------+    +---------------------+    +---------------------+
        + ^                      + ^                        + ^
  ------| |----------------------| |------------------------| |---------------------------
        v +                      v +                        v +
  +-------------------------------------------------------------------------+
  |            Socket Protocol (UDP, IP, link layer)                        |
  +-------------------------------------------------------------------------+
```

Application Protocol: data streams
  - long term keys
  - `sss::host`

Stream Protocol:
  - sending mux/demux (multiple app streams with priorities)
  - data retransmission and congestion control
  - distinguish real-time and background data
  - special streams for datagrams (dg stream, audio stream, video stream)
  - `sss::stream`

Channel Protocol: curvecp-like
  - short term keys
  - packet end-to-end encryption
  - forward secrecy
  - `sss::channel`

Pluggable congestion control: e.g. LEDBAT for file sync, Chicago for active sessions etc.
  - `sss::decongestion`

Socket Protocol level:
  - receive datagrams and demux them to channels
  - `uia::comm::socket` aka `boost::asio::udp`

### 2.1 Interface Abstractions

#### 2.1.1 Sessions

A session represents a context in which SSS runs over some underlying network protocol such as UDP or IP. Each session represents an association between two network endpoints. A session is always uniquely defined by a pair of endpoints, but the definition of an endpoint depends on the underlying protocol on which SSS runs:

 * When SSS is run atop UDP, an endpoint consists of an IP address paired with a 16-bit UDP port number. From the perspective of any given host a session is thus uniquely defined by the 4-tuple (local IP, local port, remote IP, remote port). The session tuple for the opposite host is obtained by swapping the local and remote parts of the tuple.

 * If SSS is run directly atop IP as a "native" transport alongside TCP and UDP, then an endpoint consists only of an IP address, and thus an SSS session is uniquely defined by the pair of IP addresses of the hosts involved: (local IP, remote IP). By definition there can be only one such "native" SSS session at a time between any pair of hosts.

 * If SSS is run atop some other network- or link-layer protocol, then SSS uses as its "endpoints" whatever the underlying protocols uses as an "address" or "host identifier." If SSS were to be run directly atop Ethernet, for example, then SSS’s endpoints would be IEEE MAC addresses, and a session would be uniquely defined by a pair of MAC addresses.

#### 2.1.2 Channels

The channel abstraction provides the interface between the channel protocol and the stream protocol. The channel protocol can multiplex arbitrary number of channels onto a session. The number of channels is only limited by the machine's available memory. Channel identifiers are 32-byte short-term public keys of the peer; thus, a channel is uniquely identified by the 4-tuple of (local endpoint, local channel, remote endpoint, remote channel). Each channel represents a separate instance of the SSS channel protocol resulting from a successful key exchange and feature negotiation using the negotiation protocol; SSS’s channels are therefore analogous in function to security associations in IPsec. Different channels always use independent key pairs for encryption and authentication. A given channel always uses one set of key pairs and negotiated parameters, however: when SSS needs to re-key a communication session (e.g., to ensure freshness of keys), it does so by creating a new channel through a fresh run of the negotiation protocol and terminating use of the old channel.

In SSS both parties may be trying to establish connection to each other. This may lead to two completely separate channels being set up. If this happens and peers detect it, they SHOULD migrate their streams toward the more recent channel. However, they MAY decide to not do it for increased security or logical packet separation.

Since channels rotate rather frequently (once in about 30 minutes), streams may be grouped differently onto each new set of channels, for example if a channel is set up and another channel expires, its streams may be reattached onto already existing channel.

We should provide support to pad packets to reduce vulnerability to traffic analysis. Both size and frequency of the encrypted packets should be adjustable.

Datagrams are specified to always fit in the [smallest required IPv6 datagram size](http://tcpipguide.com/free/t_IPv6DatagramSizeMaximumTransmissionUnitMTUFragment.htm), 1280 bytes (including a typical 20 byte IPv4 or 40 byte IPv6 and 8 byte UDP header, leaving 1232 to 1252 bytes for payload).

#### 2.1.3 Streams

We expect that different streams will have distinct transport characteristics which may be set or modified by the application. These include such distinct characteristic settings as:
 * Adjustable redundancy levels (trade bandwidth for latency savings)
 * Adjustable prioritization levels.

A control channel (with logical ID 0), which may be viewed as an out-of-band stream, is always available and may be used to signal state changes for the rest of the streams. The control channel will likely consist of special purpose frames (control frames).

Streams are partitioned into frames for placement into channel packets. Whenever possible, any particular packet's payload should come from only one stream. Such dedication will reduce the probability that a single packet loss will impede progress of more than one stream. When there is not sufficient data in a stream to fill a packet, then frames from more than one stream may be packed into a single packet. Such packing should reduce the packet-count-overhead, and diminish serialization latency.

#### 2.1.4 Application interface

To better support an efficient and tight binding with an application, the following current statistics are plausibly expected to be made visible to the application:
 * RTT (current smoothed estimate)
 * Packet size (including all overhead; also excluding overhead, only including payload)
 * Bandwidth (current smoothed estimate across of entire connection)
 * Peak Sustained Bandwidth (across entire connection)
 * Congestion window size (expressed in packets)
 * Queue size (packets that have been formed, but not yet emitted over a wire)
 * Bytes in queue
 * Per-stream queue size (either bytes per stream, or unsent packets, both??)

Notification should also be provided, or access for the following events (granularity of notification is TBD, and there should be no requirement on timeliness of the notifications, but any notification or status should include a best estimate of when the actual event took place):
 * Queue size has dropped to zero
 * All required ACKs have been received (connection may be closed with no transmission state loss.)
   * ACK of specific packet (section of stream?) has been received (not all streams support this. Should this be queryable, rather than a notification?)

### 2.2 Packet types

Each packet is distinguished by its magic value. Of the below types, all negotiation packets have own magic values to make negotiation level's work simpler. The rest of the packets use the single `message` packet magic and further discrimination is performed after opening the packet's crypto box.

```
packet
+--negotiation
   +--HELLO
   +--COOKIE
   +--INITIATE
+--MESSAGE
   +--data
   |  +--frame sequence
   +--forward error correction
      +--redundant data for single-packet recovery
```

## 3 The Negotiation Protocol

SSS’s negotiation protocol is responsible for setting up new channels for use by the channel and stream protocols. The negotiation protocol is responsible for performing short-term key agreement and host identity verification.

### 3.1 Basic Design Principles

The negotiation protocol is asymmetric in that the two participants have clearly delineated "initiator" and "responder" roles. The protocol supports peer-to-peer as well as client/server styles of communication, however, and the channels resulting from negotiation are symmetric and can be used by either endpoint to initiate new logical streams to the other endpoint.

The INITIATE packet from the initiator must contain proper responder cookie or be silently discarded. The more general technique of "remote storage" eliminates storage on a responder in favor of storage inside the network: the responder sends data as an encrypted authenticated message to itself via the initiator inside an opaque cookie.

#### 3.1.1 Anti-amplification Measures

First Initiator message - HELLO packet - must be bigger from the connecting initiator than the responder COOKIE reply to reduce amplification attack possibility.

The responder doesn't retransmit its first packet, the COOKIE packet. The initiator is responsible for repeating its HELLO packet to ask for another COOKIE packet.

#### 3.1.2 Replay Attacks Protection

If the attacker makes copies of a legitimate initiator's HELLO packets then the attacker will receive responder COOKIE packets without affecting the responder state; these COOKIE packets do not leak information and will be rejected by the legitimate initiator. If the attacker makes copies of other initiator packets then the copies will be rejected by this responder and by other responders. If the attacker makes copies of responder packets then the copies will be rejected by this initiator and by other initiators.

#### 3.1.3 Forward Secrecy

Protocol provides forward secrecy for the initiator's long-term public key. Two minutes after a connection is closed, the responder is unable to extract the initiator's _long-term public key_ from the network packets that were sent to that responder, and is unable to verify the initiator's long-term public key from the network packets -- that is because initiator is always using short-term public key to encrypt -- the only place where initiator's long-term public key is revealed is in Vouch subpacket, which is inside a crypto box.

Here's how the forward secrecy works. At the beginning of a connection, the responder generates a short-term public key `R'` and short-term secret key `r'`, supplementing its long-term public key `R` and long-term secret key `r`. Similarly, the initiator generates its own short-term public key `I'` and short-term secret key `i'`, supplementing its long-term public key `I` and long-term secret key `i`. Almost all components of packets are in cryptographic boxes that can be opened only by the short-term secret keys `r'` and `i'`. The only exceptions are as follows:

 * Packets from the initiator contain, unencrypted, the short-term public key `I'`. This public key is generated randomly for this connection; it is tied to the connection but does not leak any other information.
 * The first packet from the initiator contains a cryptographic box that can be opened by __`i'` and by `r`__ (not `r'`; the initiator does not know `R'` at this point). This box contains initiator's long-term public key `I` for validation against black-list by the responder.
 * The first packet from the responder contains a cryptographic box that can be opened by __`i'` and by `r`__. However, this box contains nothing other than the responder's short-term public key `R'`, which is generated randomly for this connection, and a cookie, discussed below.
 * The second packet from the initiator contains a cookie from the responder. This cookie is actually a cryptographic box that can be understood only by a "minute key" in the responder. Two minutes later the responder has discarded this key and is unable to extract any information from the cookie.
 * At the end of the connection, both sides throw away the short-term secret keys `r'` and `i'`.

Channels hold short-term keys for encrypted session. Closing a channel destroys those keys, providing forward secrecy. Channels are closed after arbitrary amount of time to destroy keys. The default channel timeout is 30 minutes plus minus a few minutes.

#### 3.1.4 Security requirements for nonces

Attached to the box is a public 24-byte nonce chosen by the sender. Nonce means "number used once." After a particular nonce has been used to encrypt a packet, the same nonce must never be used to encrypt another packet from the sender's secret key to this receiver's public key, and the same nonce must never be used to encrypt another packet from the receiver's secret key to this sender's public key. This requirement is essential for cryptographic security.

### 3.2 Negotiation Protocol Packet Format

Negotiation protocol uses 3-way message exchange to verify peer's identity and start secure channel. Actual data transfer may start already with the third packet. A faster zero-RTT connection establishment is not used to provide better forward secrecy guarantees.

#### 3.2.1 Responder Cookie format

```
ofs : sz  : description
0   : 16  : compressed nonce, prefix with "minute-k"
16  : 80  : secretbox under minute-key, containing:
    : ofs : sz  :
    : 0   : 32  : initiator short-term public key I'
    : 32  : 32  : responder short-term secret key r'
TOTAL: 96 bytes
```

When the minute key expires, the cookie could not be decrypted and will make connection attempts with this cookie ignored.

#### 3.2.2 HELLO packet format

First packet sent by the initiator willing to establish a connection. This packet is artificially padded with zeros to make it larger than the response packet, reducing amplification attacks possibility.

```
ofs : sz  : description
0   : 8   : magic
8   : 32  : initiator short-term public key I'
40  : 64  : zero
104 : 8   : compressed nonce, prefix with "cUVVYcp-CLIENT-h"
112 : 80  : box I'->R containing:
    : ofs : sz  :
    : 0   : 32  : initiator long-term public key I (for pre-auth)
    : 32  : 32  : zero
TOTAL: 192 bytes
```

#### 3.2.3 COOKIE packet format

In response to HELLO packet, the responder does not allocate any state. Instead, it encodes information about the initiator into the returned Cookie. If the initiator is willing to continue session it responds with an Initiate packet, which may contain initial message data along with identifying Cookie.

In response, responder encodes initiator's short-term public key `I'` and own short-term secret key `r'` using a special minute key, which is rotated every minute. If session isn't started within this interval, the responder will not be able to open this box and will discard the Initiate packet, thus failing session negotiation.

```
ofs : sz  : description
0   : 8   : magic
8   : 16  : compressed nonce, prefix with "cURVEcpk"
24  : 144 : box R->I' containing:
    : ofs : sz  :
    : 0   : 32  : responder short-term public key R'
    : 32  : 96  : Responder Cookie (@sa 3.2.1)
TOTAL: 168 bytes
```

#### 3.2.4 INITIATE packet format

When Initiate packet is accepted, starting a session, cookie must be placed into a cache and cleaned when minute key is rotated to avoid replay attacks.

@todo Require a congestion control SETTINGS frame inside INITIATE message before sending any stream data.

```
ofs : sz    : description
0   : 8     : magic
8   : 32    : initiator short-term public key I'
40  : 96    : Responder Cookie (@sa 3.2.1)
136 : 8     : compressed nonce, prefix with "cURVEcp-CLIENT-i"
144 : 112+M : box I'->R' containing:
ofs :   ofs : sz  :
144 :   0   : 32  : initiator long-term public key I
176 :   32  : 16  : compressed nonce, prefix with "cURVEcpv"
192 :   48  : 48  : box I->R containing Vouch subpacket:
            : ofs : sz  :
            : 0   : 32  : initiator short-term public key I'
240 :   96  : M   : message
TOTAL: 240+M+16 bytes
```

M size is in multiples of 16 between 16 and 1024 bytes.

#### 3.2.5 INITIATOR MESSAGE packet format

Responder and initiator message packets differ only in the kind of short term key and direction of encryption of the box. Each side sends packets with their own short-term public key as identifier.

```
ofs : sz   : description
0   : 8    : magic
8   : 32   : initiator short-term public key I'
40  : 8    : compressed nonce, prefix with "cURVEcp-CLIENT-m"
48  : 16+M : box I'->R' containing:
    : ofs  : sz  :
    : 0    : M   : message
TOTAL: 64+M bytes
```

M size is in multiples of 16 between 48 and 1088 bytes.

#### 3.2.6 RESPONDER MESSAGE packet format

```
ofs : sz   : description
0   : 8    : magic
8   : 32   : responder short-term public key R'
40  : 8    : compressed nonce, prefix with "cURVEcp-SERVER-m"
48  : 16+M : box R'->I' containing:
    : ofs  : sz  :
    : 0    : M   : message
TOTAL: 64+M bytes
```

M size is in multiples of 16 between 48 and 1088 bytes.

#### 3.2.7 INITIATE_MESSAGE packet

This packet type may be useful for zero-RTT session establishment. Several messages in this scenario may be not forwardly-secret, because initiator uses responder's long-term public key to box them. Only after we've received an INITIATE_ACK message with responder's short-term public key we could switch to proper forward-secret channel.

**@todo** This part needs more work to define how these messages could be mixed with normal 1-RTT session establishment, how packet loss could be dealt with if INITIATE_ACK is lost, etc. It is not crucial at this stage of development however.

## 3.3 Describe possible values of magic field

**@todo** These values are initially based on CurveCP, with letter case swapped.

For packet types:
 * HELLO - "qVNq5xLh"
 * COOKIE - "rl3Anmxk"
 * INITIATE - "qVNq5xLi"
 * MESSAGE - "rl3q5xLm"
 * INITIATE_MESSAGE - "qVNq5xLo"

## 4 Channel Protocol

Channel protocol provides independently encrypted packetization for streams of data. Channel protocol multiplexes streams, provides packet acknowledgement, congestion control and provides
out-of-band signaling for streams management.

Figure 2: Channel protocol packet layout
```
ofs : sz : descriptions
  0 :  2 : Source Port            --+
  2 :  2 : Destination Port         | UDP Header
  4 :  2 : Length                   | 8 bytes
  6 :  2 : Checksum               --+
  8 :  8 : Packet magic           --+ Channel
 16 : 32 : Short-term public key    | Header
 48 :  8 : Compressed nonce       --+ 48 bytes
 56 :  X : Message in crypto box
TOTAL: 56 + X bytes
```

As a final step in session negotiation channel layer sets up a decongestion strategy. For this the INITIATE packet *must* contain a SETTINGS frame before all other frames of data, laying out options as requested by the initiator. A responder not willing to accept these options may RESET and CLOSE the stream. (**@todo** Make possible to progress forward by returning other options in counter-offer? Could be included in RESET or CLOSE frame.)

### 4.1 MESSAGE box format

After decrypting, message box becomes a plaintext payload block.

A *non-FEC* packet is a data packet and consists of one or more frames. Each frame has its type as first byte. Each frame type is described below. A *FEC* packet consists of a XOR of all zero-padded packets in this FEC group and can be used to recover one lost packet in this FEC group.

Non-FEC packet payload consists of:
 * A packet header (see 4.1.1)
 * One or more tagged frames (see 4.2)
 * Zero-padding. This padding produces a total message length that is a multiple of 16 bytes, at least 16 bytes and at most **1168 bytes**. This accounts for 40 bytes of IPv6 header, 8 bytes of UDP header and 64 bytes of packet header and cryptobox overhead. The total size of the datagram is thus limited to 1280 bytes - the 1280-byte datagrams are allowed by IPv6 on all networks.

Note: When describing data fields the C-like type notation is used, where
 * `uint8_t` specifies unsigned 8-bit quantity (an octet)
 * `big_uint16_t` specifies unsigned 16-bit quantity in network (big-endian) order
 * `big_uint32_t` specifies unsigned 32-bit quantity in network (big-endian) order
 * `big_uint48_t` specifies unsigned 48-bit quantity in network (big-endian) order
 * `big_uint64_t` specifies unsigned 64-bit quantity in network (big-endian) order

#### 4.1.1 Packet header format

Every packet (FEC or non-FEC) starts with a packet header, describing the packet type and sequence number. 

Packets may carry optional version field. Parties use this field to negotiate supported functionality of each end. Once endpoints have learned their peers' capabilities, the version field need not be transmitted. If a packet with version field included has been ACKed by the other side, endpoint can safely assume version field has been seen and stop transmitting it.

Figure 3: Packet header
```
    ofs :       sz : description
      0 :        1 : Flags (000fssgv)
      1 :        2 : Version (optional, if v bit is set)
    1,3 :        1 : FEC group (optional, if g bit is set)
1,2,3,4 :  2,4,6,8 : Packet sequence number (depending on ss bits)
```

* Flags `uint8_t`:
  * v - version field present
  * g - FEC group present
  * ss - size of packet sequence number field
    * 00 = 2 bytes
    * 01 = 4 bytes
    * 10 = 6 bytes
    * 11 = 8 bytes
  * f - this is last FEC group packet, in this case packet contains XOR over all zero-padded previous packets in given FEC group (bit g must also be set)
* Protocol version number (optional, `big_uint16_t` when present)
* FEC group number (optional, `uint8_t` when present)
* Packet sequence number (variable size, either `big_uint16_t`, `big_uint32_t`, `big_uint48_t` or `big_uint64_t`)

If FEC is not used, the FEC group byte is not needed. The g bit serves as FEC enable flag.

Shortest packet header is thus only 3 bytes long: zero flags and 2-byte packet sequence number.

### 4.2 Framing

**@todo might need to move framing until after section 5, after stream IDs are explained.**

Frames are stream containers within a channel packet. Streams contents are sliced into frames, which may contain stream data from one or more streams and other control information. Frames use tagged chunks, where each chunk follows a certain format with a header and content part.
```
+--------+.........+--------+.........+
| Type   | Payload | Type   | Payload |
+--------+.........+--------+.........+
```
Frames are inside the channel message cryptobox, not visible to any eavesdroppers.

#### 4.2.1 Frame types
```
 Type value | Frame type
------------+------------------
          0 | EMPTY
          1 | STREAM/ATTACH
          2 | ACK
          3 | PADDING
          4 | DECONGESTION
          5 | DETACH
          6 | RESET
          7 | CLOSE
          8 | SETTINGS
          9 | PRIORITY
```

#### 4.2.2 EMPTY frame

Figure N: Empty frame layout
```
         ofs :                  sz : description
           0 :                   1 : Frame type (0 - EMPTY)
```

Empty frame can be used to complete padding where PADDING frame doesn't fit. I.e. in 1- and 2-byte
remainders. It has lowest priority and PADDING frame is preferred.

#### 4.2.3 STREAM frame

Stream frame is used to transfer data on each individual stream. It also serves as an ATTACH packet
to initiate a new stream.

Figure N: Stream frame layout
```
         ofs :                  sz : description
           0 :                   1 : Frame type (1 - STREAM)
           1 :                   2 : Flags (niuooodf0000000r)
           3 :                   4 : Stream ID
           7 :                   4 : Parent Stream ID (optional, when INIT (i) bit is set)
          11 :                  24 : Stream USID (optional, when USID (u) bit is set)
    11,15,35 : 0,2,3,4,5,6,7,8 (O) : Offset in stream (depending on OFFSET (ooo) bits)
(11,15,35)+O :                   2 : Data length (optional, when DATA LENGTH (d) bit is set) D
(13,17,37)+O :                   D : Data
```

Flags: FIN, INIT, USID, OFFSET, DATA LENGTH, NOACK

 * When `i = INIT, 0x4000` bit is set, this frame initiates the stream by providing stream and parent unique IDs.
 * When `u = USID, 0x2000` bit is set, this `INIT` frame includes full stream Unique ID, for means of reattachment of pre-existing stream to a channel. `USID` bit can only be set when `INIT` bit is set.
 * When `f = FIN, 0x0100` bit is set, this frame marks last transmission on this stream in this direction.
 * `ooo = OFFSET` bits encode length of the stream offset field. A 0, 16, 24, 32, 40, 48, 56, or 64 bit unsigned number specifying the byte offset in the stream for this block of data. 000 corresponds to 0 bits and 111 corresponds to 64 bits. (@todo Should offset be always present?)
 * When `d = DATA LENGTH, 0x0200` bit is set, this frame has a limited number of bytes for this stream, provided in length field, otherwise stream data occupies the rest of the packet.
 * When `n = NOACK, 0x8000` bit is set, this frame does not require acknowledgement from the receiver. Essentialy this frame's data has been removed from waiting-for-ACK queue after sending, and so the sender does not care. If there are other frames in this packet, they still might require acknowledgement.
 * When `r = RECORD, 0x0001` bit is set, this frame marks end of the record in the stream data. Streams support pushing marked records which can then be read as a single entity by the receiving side.

If `FIN` bit is set, stream data length may be zero. Otherwise, data length must be non-zero.

Both `INIT` and `FIN` bits may be set at the same time. In this case data length must be non-zero.

Possible combinations of bits:
```
INIT
INIT,USID
FIN
INIT,FIN
INIT,USID,FIN
```

Given our initiator state from negotiation and next free stream id (32 bits) we can know what LSID from the other side will be - if we're initiator, then other end LSID is our LSID+1, otherwise other end LSID is our LSID-1.

We need unique USID for this stream and USID for its parent stream to inititate a new stream regardless of channel switching. Parent must be already attached to initiate a sub-stream, so LSID is enough to distinguish parent stream in wire protocol, even though USID might have been used internally.

Stream immediately starts sending data, so receiver must be able to start reception. Some sort of window should be used to keep flow control. First message in the stream may be borrowing the window from its parent stream until it establishes own window.

When stream offset is not specified, it is considered to be zero. This is useful for small short lived streams which just spit a small chunk of data in single packet before closing down, or for sending datagrams - self-contained chunks of data with no offset.

When data length is missing, data extends until the end of this packet and no other frames are present.

#### 4.2.4 ACK frame

The ACK frame is sent to inform the peer which packets have been received, as well as which packets are still considered missing by the receiver (the contents of missing packets may need to be re-sent).

Figure 4: ACK frame layout
```
ofs : sz : description
  0 :  1 : Frame type (2 - ACK)
  1 :  1 : Sent entropy
  2 :  1 : Received entropy
  3 :  1 : Number of missing packets
  4 :  8 : Least Unacked Packet
 12 :  8 : Largest Observed
 20 :  4 : Largest Observed Delta Time
 24 :  X : Missing packets NACK (variable length, may be empty)
```

Data in an ACK frame is divided logically into two sections:

##### Sent Packet Data

 * Sent Entropy `uint8_t`: Cumulative hash of entropy in all sent packets up to the packet with sequence number one less than the *least unacked packet*.
 * Least Unacked `big_uint64_t`: The smallest sequence number of any packet for which the sender is still awaiting an ack. If the receiver is missing any packets smaller than this value, the receiver should consider those packets to be irrecoverably lost.

##### Received Packet Data

 * Received Entropy `uint8_t`: Cumulative hash of entropy in all received packets up to the largest observed packet.
 * Largest Observed `big_uint64_t`:
   * If the value of Missing Packets includes every packet observed to be missing since the last ACK frame transmitted by the sender, then this value shall be the largest observed sequence number.
   * If there are packets known to be missing which are not present in Missing Packets (due to size limitations), then this value shall be the largest sequence number smaller than the first missing packet which this ACK does not include.
   * If multiple consecutive packets are lost, the value of Largest Observed may also appear in Missing Packets.
 * Largest Observed Delta Time `big_uint32_t`: Time elapsed in microseconds from when largest observed was received until this Ack frame was sent.
 * Num Missing `uint8_t`: Number of missing packets between largest observed and least unacked. **@todo Should it be number of entries in missing packets array or actual number of missing packets? Array count seems safer and you can infer total missing by summing up all entries.**
 * Missing Packets `(big_uint48_t+big_uint16_t)[]`: A series of the lower 48 bits of the sequence numbers of packets which have not yet been received (NACK).
   * RLE encoded with higher 48 bits containing the lower 48 bits of the sequence number and lower 16 bits containing the length of the run starting with this sequence number.
```
ofs : sz : description
  0 :  6 : Missing Packet lower 48 bits of sequence number
  6 :  2 : Number of missing packets
```

Number of missing packets in a NACK run cannot be zero. **@todo** Might use last entry with zero run length as indication that NACK run has been shortened, although it is not necessary.

It is expected that with regular loss rate and packet rate ACK frames will often be the minimal size (24 bytes), and only from time to time contain one or two missed packets. On bad or lossy connections the ACK frame might become big enough to have its own separate full-sized packet.

**@todo** Add graphical explanations for ACK packet fields (least unacked/largest observed).

#### 4.2.5 PADDING frame

Padding frame indicates padding data within a packet, used to counter traffic analysis attacks. Ideally all packets should be padded to have same final length of 1280 bytes. However, in case of slow connection it may be preferential to not pad the data packets or pad them to a shorter common length.
Padding frame contents may be anything and are simply ignored. It is recommended to explicitly initialize this padding data area with either random or zero bytes and not leave any old data that might leak information.
The receiver is *advised to* simply skip over this data without trying to interpret or save it in any way. After processing the packet, packet memory *should* be zeroed.

Figure 5: Padding frame layout
```
ofs : sz : description
  0 :  1 : Frame type (3 - PADDING)
  1 :  2 : Length of padding L
  3 :  L : Padding data
```

Padding packet may be as small as 3 bytes - type byte and length field, which is zero in this case.

Take note of the rounding requirements - all padding together *must* pad message size to an integer multiple of 16 bytes.

#### 4.2.6 DECONGESTION frame

Decongestion feedback frame contents are specific to chosen decongestion method in the channel. Frame format for several implemented methods will be listed here.

Figure 6: Decongestion frame layout
```
ofs : sz : description
  0 :  1 : Frame type (4 - DECONGESTION)
  1 :  1 : Subtype
  2 :  X : Method specific contents
```

##### 4.2.6.1 Congestion control feedback for TCP Cubic

Similar to TCP protocol, packet loss and receive window size are provided.

Figure 7: TCP decongestion frame layout
```
ofs : sz : description
  0 :  1 : Frame type (4 - DECONGESTION)
  1 :  1 : Subtype (1 - TCP CUBIC)
  2 :  2 : Num lost packets
  4 :  2 : Receive window size
```
 * Num lost packets `big_uint16_t`: The number of packets lost over the lifetime of this connection. This may wrap for long-lived connections.
 * Receive window `big_uint16_t`: The TCP receive window.

##### 4.2.6.2 Congestion control feedback for CurveCP Chicago

Chicago updates with RTT times as seen by the far end. This is not required for operation of Chicago protocol, which handles everything on the near side, but is included for complete information for the far end.

Figure 8: Chicago decongestion frame layout
```
ofs : sz : description
  0 :  1 : Frame type (4 - DECONGESTION)
  1 :  1 : Subtype (2 - Chicago)
  2 :  4 : RTT High
  6 :  4 : RTT Low
 10 :  4 : RTT Average
 14 :  4 : RTT Mean deviation
```
 * Highest RTT `big_uint32_t`: **@todo**
 * Lowest RTT `big_uint32_t`: **@todo**
 * Average RTT `big_uint32_t`: **@todo**
 * RTT mean deviation `big_uint32_t`: **@todo**

##### 4.2.6.3 Congestion control feedback for UDP LEDBAT

```
ofs : sz : description
  0 :  1 : Frame type (4 - DECONGESTION)
  0 :  1 : Type
  1 :  1 : Subtype (3 - LEDBAT)
  .......
```

**@todo**

##### 4.2.6.4 Congestion control feedback for WebRTC Inter-arrival

Figure 10: Inter-arrival decongestion frame layout
```
ofs : sz : description
  0 :  1 : Frame type (4 - DECONGESTION)
  1 :  1 : Subtype (4 - Inter-arrival)
  2 :  2 : Num lost packets
  4 :  1 : Received
  5 :  6 : Smallest Received Packet
 11 :  8 : Smallest Delta Time
 19 :  2 : Packet Delta
 21 :  4 : Packet Time Delta
```
 * Num lost packets `big_uint16_t`: The number of packets lost over the lifetime of this connection. This may wrap for long-lived connections.
 * Received `uint8_t`: Number of received packets in this update.
 * Smallest Received Packet `big_uint48_t`: The lower 48 bits of the smallest sequence number represented in this update.
 * Smallest Delta Time `big_uint64_t`: Delta time from connection creation when the above packet was received.
 * Packet Delta `big_uint16_t`: Sequence number delta from the Smallest Received Packet. Always followed immediately by a corresponding Packet Time Delta.
 * Packet Time Delta `big_uint32_t`: Time delta from smallest time when the preceding packet sequence number was received.

#### 4.2.7 DETACH frame

Detach frame allows stream to detach from current channel without shutting down the stream. Detaching informs the other side that this stream should not be torn down, but it will not send more data on this channel. Stream may be subsequently reattached, if needed, to this or some other channel. The stream is NOT closed or half-closed after detach, however it cannot be used for data transfer until re-attached.

Detach frame only contains LSID of stream that will be detached.

Figure 11: Detach frame layout
```
ofs : sz : description
  0 :  1 : Frame type (5 - DETACH)
  1 :  4 : Stream Local ID (LSID) in sender ID space
```
 * Stream Local ID `big_uint32_t`: LSID of the stream to detach (in sender's ID space)

#### 4.2.8 RESET frame

Abort stream.

The RESET frame allows for abnormal termination of a stream. When sent by the creator of a stream, it indicates the creator wishes to cancel the stream. When sent by the receiver of a stream, it indicates an error or that the receiver did not want to accept the stream, so the stream should be closed.

Figure 12: Reset frame layout
```
ofs : sz : description
  0 :  1 : Frame type (6 - RESET)
  1 :  4 : Stream Local ID (LSID) in sender ID space
  5 :  4 : Error code
  9 :  2 : Reason phrase length R
 11 :  R : Reason phrase (variable length, may be 0)
```
 * Stream Id `big_uint32_t`: LSID of the stream (in sender's ID space).
 * Error code `big_uint32_t`: Error code which indicates why the stream is being closed. (**@todo Add a table of error codes!**)
 * Reason phrase length `big_uint16_t`: Length of the reason phrase. This may be zero if the sender chooses to not give details beyond the error code.
 * Reason phrase: A UTF-8 encoded optional human-readable explanation for why the connection was closed. It is **not** zero-terminated.

#### 4.2.9 CLOSE frame

Close channel.

**@todo Immediate close and non-immediate (goaway) close?**

Figure 13: Close frame layout
```
ofs : sz : description
  0 :  1 : Frame type (7 - CLOSE)
  1 :  4 : Error code
  5 :  2 : Reason phrase length R
  7 :  R : Reason phrase (variable length, may be 0)
7+R :  X : Final ACK frame (variable length)
```
 * Error code `big_uint32_t`: Error code which indicates why the connection is being closed.
 * Reason phrase length `big_uint16_t`: Length of the reason phrase. This may be zero if the sender chooses to not give details beyond the error code.
 * Reason phrase: An optional human-readable explanation for why the connection was closed.
 * AckFrame: A final ack frame, letting the peer know which packets had been received at the time the connection was closed. A complete ACK frame is contained within this field and can be parsed with normal frame parser.

### 4.2.10 SETTINGS frame

Allow to setup some connection parameters.

Currently supported options:
  * FEC
  * Congestion control algorithm

Settings are in a list of integer tags and associated values. Types of values are predefined.

```
ofs : sz : description
  0 :  1 : Frame type (8 - SETTTINGS)
  1 :  2 : Number of settings N
  3 :  X : Settings tags
```
 * Number of settings `big_uint16_t`: Number of setting pairs in this frame
 * Each setting consists of `big_uint16_t` tag and associated value.

List of negotiation tags:
```
 Tag | Meaning                      | Value Type
-----+------------------------------+--------------
   1 | FEC                          | uint8_t
   2 | Congestion control algorithm | big_uint16_t
```
 * For FEC the `uint8_t` value is treated as a boolean flag, with 0 indicating NO and 1 indicating YES for FEC use in this session.
 * For CC the `big_uint16_t` value is treated as an enum of used CC algorithms with following values:
   * 0 - no CC
   * 1 - TCP CUBIC
   * 2 - Chicago
   * 3 - LEDBAT
   * 4 - Inter-arrival

Tags must be sorted in the order of increasing tag number. No duplicate tags are presently allowed.

### 4.2.11 PRIORITY frame

PRIORITY frame indicates to the receiver a priority of processing frame data for a given stream.
It is only a hint. The receiver should make best effort to process given stream's data in accordance with relative priority given (streams with priority 0 should always be processed first, then streams with priority 1 and so on).

```
ofs : sz : description
  0 :  1 : Frame type (9 - PRIORITY)
  1 :  4 : Stream Local ID (LSID) in sender ID space
  5 :  4 : Priority value
```
 * Priority value is a `big_uint32_t` with 0 for maximum stream priority and maximum uint32_t value for minimum stream priority.

### 4.3 Frame assembly

Frame assembly deals with allocating available packet buffer length to various frames depending
on the priority and urgency of sending them out.

- Send higher priority streams first
- Distribute bandwidth approximately equally between same-priority streams
- Prioritise ACK frames to ensure progress is made
- Establishing lower-priority streams may be postponed

Frame types sorted by priority - highest to lowest:
```
SETTINGS
ACK
RESET
PRIORITY
DECONGESTION
DETACH
CLOSE
STREAM/ATTACH
PADDING
EMPTY
```

Frame assembly needs to account for both channel and stream layers requests.

### 4.3.1 SETTINGS
- Layer: Channel

### 4.3.2 ACK
- Layer: Channel

### 4.3.3 RESET
- Layer: Stream

### 4.3.4 PRIORITY
- Layer: Stream
- check https://dl.dropboxusercontent.com/s/1pdteaj3l1yp3lu/2015-02-19%20at%2021.49.jpg

- Priority must be relative to parent?
- Child streams cannot progress before their parent.

### 4.3.5 DECONGESTION
- Layer: Channel

- check https://dl.dropboxusercontent.com/s/4k7s83fmeux1b87/2015-02-19%20at%2021.50.jpg
- Send up to a lesser of stream window and channel window

### 4.3.6 STREAM/ATTACH
- Layer: Stream

### 4.3.7 DETACH
- Layer: Stream

### 4.3.8 CLOSE
- Layer: Channel

### 4.3.9 PADDING
- Layer: Framing



Trying to fit: if higher-priority buffer does not fit into current packet, it is either split 
if possible or postponed to the next packet with increased priority. Priority is increased to eventually put this frame first in the packet or signal impossibility of sending that frame.


## 5 Stream Protocol

Streams are independent sequences of uni- or bi­directional data flows cut into stream frames. Streams group logically communications between two parties. Streams can be created by either peer; can concurrently send data interleaved with other streams; and can be cancelled.

Streams must be attached to channels to be able to send. Streams that need to send data attach onto a channel based on their integer priority. Stream with absolute largest priority on the channel wins and will always send first as long as it has data to send.

Streams have a unique ID, used to distinguish this stream after switching to a newly established channel. Streams in channels keep their global IDs and continue delivering data even after channel switch.

### Stream IDs - USID

SSS assigns each logical stream a permanent Unique Stream Identifier (USID) when the stream is first created, and uses this identifier to refer to the stream if it becomes necessary to detach the stream from its original channel or migrate it to another channel. A USID consists of two components, a 16-byte cryptographic half-channel identifier and a 64-bit stream counter.

Figure X: USID structure
```
 ofs : sz : description
   0 : 16 : cryptographic half-channel identifier
  16 :  8 : big_uint64_t stream counter
TOTAL: 24 bytes
```

Each channel has two half-channel identifiers, one for each direction of information flow, both of which the negotiation protocol computes for the channel during key exchange. Must be a reproducible algorithm able to generate the half-channel ID both predictable and cryptographically strong. Using short-term keys of both parties is recommended.

Original half-channel identifier calculation from SST
```
calc_key(master, initiator_hashed_nonce, responder_nonce, byte which, int keylen)
{
    master_hash = crypto::sha256::hash(master); // Use master_hash as hmac key
    crypto::hash hmac(master_hash).update(initiator_hashed_nonce+responder_nonce+{which});
    crypto::hash::value key;
    hmac.finalize(key);
    key.resize(keylen);
    return key;
}

// Set up the new channel IDs
tx_chan_id = calc_key(master_secret, responder_nonce, initiator_hashed_nonce, 'I', 128/8);
rx_chan_id = calc_key(master_secret, initiator_hashed_nonce, responder_nonce, 'I', 128/8);
```

Which of a channel’s half-channel identifiers is assigned to a given stream depends on which participant host originated the stream. **@todo** What about having the same half-channel ID in both directions? **endtodo** The stream counter value, in turn, distinguishes among streams created by that host during the channel’s lifetime.
Although in theory every stream has an USID, in practice for most short-lived streams that remain attached to their original channel throughout their lifetimes, the stream’s USID is never actually transmitted or used by the wire protocol. Within the context of a particular channel, SSS normally identifies streams using shorter 32-bit Local Stream Identifiers or LSIDs, described in the next section.

### Stream IDs - LSID

LSID is similar to QUIC stream numbers - it's a 32-bit number, started from different start values by initiator and responder.

At a given point in time a stream may have between zero and four attachments, two for each direction of information flow. Each attachment binds the stream to a particular channel and associates a 32-bit Local Stream Identifier (LSID) to the stream for the purpose of transmitting stream data over that channel. The scope of an LSID is local to a particular channel and flow direction: each endpoint host on a channel has its own LSID space, within which it may assign LSIDs independently of the other endpoint and of other channels.

SSS allows a stream to have up to two attachments in each direction so that a host can transmit data on a stream continuously using one attachment while setting up a second attachment to a different channel, in order to migrate streams from one channel to another smoothly and transparently to the application. Hosts may detach active streams not only to migrate them but also to free up LSID space; long-lived but inactive streams may remain unattached in one or both flow directions for arbitrary periods of time.

### 5.1 Channel root stream (LSID 0)

Root stream does not have a unique ID and therefore is always implicitly started on a channel. It is used to spawn first application-level stream and for out-of-band signaling about other streams.

Whenever a pair of SSS hosts set up a new channel via the negotiation protocol, the hosts implicitly create a special stream for the channel called the channel root. A channel’s root stream is always attached to the channel with an LSID of 0 in each direction, and never detaches or migrates to other channels. The channel itself terminates once its root stream is closed in both directions.
Applications are not generally aware of the existence of channel root streams at all: channel roots merely provide an outermost context in each channel that the stream layer uses to exchange control messages and initiate (or migrate in) other streams on behalf of applications.

### Service request streams (**@fixme** LSIDs 1 and 2)

When the application makes a connect request to open a new top-level stream to a given target host and service, the stream protocol on the initiating host creates a service request stream as a substream of a suitable channel’s root stream. The initiating stream protocol then sends a service request message on this new stream. Initiator starts this stream with LSID 1, responder uses LSID 2 for the same stream. Responder is always on the even side with regards to LSIDs, while initiator is on the odd side.

Service request streams also allow to query the application about supported service endpoints.

Service request streams operate with a simple format of commands which include:
 * Query list of services
 * Respond with a list of services
 * Query protocols of a given service
 * Respond with a list of protocols for a service
 * Request connection to given service and protocol
 * Respond with connection results

All requests and responses start with a service code. Service codes are 2 bytes in length. First byte is 0x01 for requests and 0x02 for responses. Second byte determines the request or response type and is specified in sections below.

#### Request connection to given service and protocol

```
ofs : sz : description
  0 :  1 : Type (1)
  1 :  1 : Request type (1)
  2 :  2 : Size of service name S
  4 :  S : Service name
4+S :  2 : Size of protocol name P
6+S :  P : Protocol name
```

#### Respond with connection results

```
Success & error replies:
ofs : sz : description
  0 :  1 : Type (2)
  1 :  1 : Reply type (1)
  2 :  4 : Status code
  6 :  2 : Size of reply R
  8 :  R : Reply string
```

#### Query list of services

```
ofs : sz : description
  0 :  1 : Type (1)
  1 :  1 : Request type (2)
```

#### Respond with a list of services

```
Success reply:
ofs : sz : description
  0 :  1 : Type (2)
  1 :  1 : Reply type (2)
  2 :  4 : Status code
  6 :  4 : Reply count C, N = 0..<C
 10 :  2 : Reply N size of service name S --+ Reply count C
 12 :  S : Reply N service name           --+ times
```

```
Error reply:
ofs : sz : description
  0 :  1 : Type (2)
  1 :  1 : Reply type (2)
  2 :  4 : Status code
  6 :  2 : Size of reply R
  8 :  R : Reply string
```

#### Query protocols of a given service

```
ofs : sz : description
  0 :  1 : Type (1)
  1 :  1 : Request type (3)
  2 :  2 : Service name length S
  4 :  S : Service name
```

#### Respond with a list of protocols for a service

```
Success reply:
 ofs : sz : description
   0 :  1 : Type (2)
   1 :  1 : Reply type (3)
   2 :  4 : Status code
   6 :  2 : Service name length S
   8 :  S : Service name
 8+S :  4 : Protocol name count C, N = 0..<C
12+S :  2 : Reply N size of protocol name P --+ Reply count C
14+S :  P : Reply N protocol name           --+ times
```

```
Error reply:
ofs : sz : description
  0 :  1 : Type (2)
  1 :  1 : Reply type (3)
  2 :  4 : Status code
  6 :  2 : Size of reply R
  8 :  R : Reply string
```

### 5.2 Starting new stream.

New stream is started by posting STREAM frame with INIT flag set.
This is essentially all that's needed for starting a stream. When starting a new stream `INIT` bit must be set. When this bit is set, parent stream LSID is transferred in STREAM frame, providing hereditary structure to streams. Since LSID 0 is always open on a channel, first created streams specify it as the parent.

All streams are identified by an unsigned 32-bit integer. Streams initiated by an initiator use odd numbered stream identifiers; those initiated by the responder use even numbered stream identifiers. A stream identifier of zero MUST NOT be used to establish a new stream.

Given our initiator/responder state from negotiation and next free stream ID (32 bits) we can know what the LSID from the far side will be - if we're initiator, then far end LSID is near end LSID+1, otherwise far end LSID is near end LSID-1.

The identifier of a newly established stream MUST be numerically greater than for all previously established streams on that channel. An endpoint that receives an unexpected stream identifier MUST respond with a channel error of type PROTOCOL_ERROR.

Stream identifiers cannot be reused within a connection.  Long-lived connections can cause an endpoint to exhaust the available range of stream identifiers.  A client that is unable to establish a new stream identifier can establish a new channel for new streams.

### 5.3 Initiating substreams

Technically, spawning a substream does not differ from spawning initial streams. Substreams are started by posting STREAM frame with INIT flag set. Parent LSID field indicates the parent stream to spawn from. For non-initial streams parent LSID field is non-zero.

Initiating substream borrows on parent stream's window buffer space. **@todo**

### 5.4 Attaching a stream to channel

Streams are attached by posting STREAM frame with INIT and USID flags set. USID specifies a unique identifier already known to peer to indicate that this stream is resuming on a new channel.
New channel may have a different direction of initiator/responder so LSID allocation is handled by channel layer.

**@todo**

### 5.5 Detaching a stream from channel

Stream is detached from channel by sending a DETACH frame. Stream remains open and can be reattached on same or different channel to continue transmission. Unacked and untransmitted packets from the stream are returned to stream buffer.

**@todo**

### 5.6 Stream data exchange

Once a stream is created, it can be used to send arbitrary amounts of data. Generally this means that a series of frames will be sent on the stream until a frame containing the FIN bit is sent. Once the FIN has been sent, the stream is considered to be half­-closed in this direction.

**@todo**

### 5.7 Stream half­-close

When one side of the stream sends a frame with FIN bit set, the stream is considered to be half-­closed from that side. The sender of the FIN is indicating that no further data will be sent by the sender on this stream. When both sides have half­-closed, the stream is considered to be closed.

### 5.8 Stream close

When both sides have indicated their desire to stop sending on the stream, stream becomes closed. Closed stream cannot be reopened. Stream LSIDs will not be reused in this sesssion.

**@todo**

### Listening on a stream

When client starts listening on a stream, system puts it into the list of streams awaiting connection with client provided service and protocol names. Awaiting streams are expecting a connection request record with service and protocol name matching their own.

**@todo**

###### dg_stream, audio_stream, video_stream subtypes

These substreams only exist in the application layer and provide specific methods of assembling the data frames. They are oriented at real-time best-effort delivery communications.

**@todo** More details...

## 6. Testing

**@todo**

Tests to run:
 * Speed of establishing connection
 * Resilience to packet loss
 * Behavior in edge cases (too much packet loss, competing LEDBAT or TCP streams)

## 7. References

 * [SST Structured Streams Transport](http://pdos.csail.mit.edu/uia/sst/)
 * [SPDY protocol](http://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3-1)
 * [SPDY v4 Draft](https://github.com/grmocg/SPDY-Specification/blob/gh-pages/draft-mbelshe-spdy-00.txt)
 * [QUIC protocol](@todo)
 * [RDP Reliable Data Protocol](https://tools.ietf.org/html/rfc908)
 * [ECN in IP](https://tools.ietf.org/html/rfc3168)
 * [TCP extensions for highperf](https://tools.ietf.org/html/rfc1323)
 * [SCTP](http://en.wikipedia.org/wiki/Stream_Control_Transmission_Protocol)
 * [DTLS](http://en.wikipedia.org/wiki/Datagram_Transport_Layer_Security)
 * [SSU (Secure Semireliable UDP, an i2p protocol)](https://geti2p.net/en/docs/transport/ssu)
