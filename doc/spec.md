Structured Streams
==================

## 1 Introduction

SSU builds on SST, SPDY, QUIC and CurveCP protocols.

SST provides the basis for the following set of features:
 * Multiplex many application streams onto one network connection
 * Gives streams hereditary structure: applications can spawn lightweight streams from existing ones
   * Efficient: no 3-way handshake on startup or TIME-WAIT on close
   * Supports request/response transactions without serializing onto one stream
   * General out-of-band signaling: control requests already in progress
 * Both reliable and best-effort delivery in a semantically unified model
   * supports messages/datagrams of any size: no need to limit size of video frames, RPC responses, etc.
 * Dynamic prioritization of application's streams
   * e.g., load visible parts of a web page first, change priorities when user scrolls
 * End-to-end cryptographic security

SPDY and QUIC extend with packet framing, encoding and a set of goals to achieve (see below).

CurveCP adds non-transparent cryptoboxes for all crucial contents and a session initiation protocol.

Note: Integers are in network (big-endian) order. All numbers are unsigned.

### Goals (@sa QUIC)
We’d like to develop a transport that supports the following goals:
  1. Widespread deployability in today’s internet (i.e., makes it through middle-boxes; runs on common user client machines without kernel changes, or elevated privileges)
  2. Reduced head-of-line blocking due to packet loss (losing one packet will not generally impair other multiplexed streams)
  3. Low latency (minimal round-trip costs, both during setup/resumption, and in response to packet loss)
    a. Significantly reduced connection startup latency (Commonly zero RTT connection, cryptographic hello, and initial request(s))
    b. Attempt to use Forward Error Correcting (FEC) codes to reduce retransmission latency after packet loss.
  4. Improved support for mobile, in terms of latency and efficiency (as opposed to TCP connections which are torn down during radio shutdowns)
  5. Congestion avoidance support comparable to, and friendly to, TCP (unified across multiplexed streams)
    a. Individual stream flow control, to prevent a stream with a fast source and slow sink from flooding memory at receiver end, and allow back-pressure to appear at the send end.
  6. Privacy assurances comparable to TLS (without requiring in-order transport or in-order decryption)
  7. Reliable and safe resource requirements scaling, both server-side and client-side (including reasonable buffer management and aids to avoid facilitating DoS magnification attacks)
  8. Reduced bandwidth consumption and increased channel status responsiveness (via unified signaling of channel status across all multiplexed streams)
  9. Reduced packet-count, if not in conflict with other goals.
  10. Support reliable transport for multiplexed streams (can simulate TCP on the multiplexed streams)
  11. Efficient demux-mux properties for proxies, if not in conflict with other goals.
  12. Reuse, or evolve, existing protocols at any point where it is plausible to do so, without sacrificing our stated goals (e.g., consider LEDBAT, DCCP, TCP minion)

#### WHY NOT USE SCTP over DTLS?

One recurring question is: Why not use SCTP (Stream Control Transmission Protocol) over DTLS (Datagram Transport Layer Security)? SCTP provides (among other things) stream multiplexing, and DTLS provides SSL quality encryption and authentication over a UDP stream. In addition to the fact that both of these protocols are in various levels of standardization, this combination is currently on a standard track, and described in this Network Working Group Internet Draft.
The largest visible issue with using these protocols relates to our goals in the area of connection latency, and is perhaps the most critical conflicting element. In addition, we can also anticipate issues in bandwidth efficiency that may reduce our ability to achieve the goals.

[channels](http://www.asciidraw.com/#5074225503160675365/76617341)

One problem (at least with normal implementations) is that the application cannot access the packets coming after a lost packet until the retransmitted copy of the lost packet is received. This causes problems for real-time applications such as streaming media, real-time multiplayer games and voice over IP (VoIP) where it is generally more useful to get most of the data in a timely fashion than it is to get all of the data in order.

A packet is routinely delayed when a packet is lost, such as due to congestion, and it must be retransmitted. A better multiplexed transport should delay only one stream when a single packet is lost.

### 1.1 Packet loss

Packet loss in the Internet is broadly estimated to be in the range of 1-2% of all packets. These numbers have been confirmed by tests of clients, such as Chrome, recording stats for test streams of UDP packets to server farms around the world. It is extremely unlikely that over 100 packets can be received without a loss, and certainly not 200.
The primary cause for packet loss is believed to be congestion, where routers perform switching operations, and output buffer sizes are exceeded. This issue is fundamental to the design of the Internet, and TCP, where packet loss is used as a signal of congestion, and the protocol is required to respond by reducing the flow across the congested path. Packet loss can also be caused by analog factors on transmission lines, but such losses are believed to be much lower in rate, and hence negligible.

Packet loss will be handled by two mechanisms: Packet-level error correcting codes, and lost data retransmission. The ultimate fallback when all else fails will be retransmission of lost data. When data is retransmitted as a response to a lost packet, the original packet is not retransmitted. Instead, the encapsulated data is placed in a new packet, and that new packet is sent.

For reduced latency of shorter streams, and for the tail-end of some (all?) streams, redundant information may be added by the protocol to facilitate error correction, and reduce the need for retransmission. For larger streams, where serialization latency is deemed to be the dominant factor (by the application that constructs and send the stream), the use of FEC redundancy may be reduced or eliminated for most packets of the stream.

## 2 Design Overview

Figure 1: Protocol Architecture [source](http://www.asciidraw.com/#5612283093966789321/1974720956) [advanced diagram](https://dl.dropboxusercontent.com/s/6ck76plbu7lrun3/2014-10-02%20at%2012.21.png?dl=0)
```
  +------------------------------------------------------------------------+
  |                      Application Protocol                              |
  +------------------------------------------------------------------------+
                     ^+                                     +^
  -------------------|| Streams-----------------------------||----------------------------
                     +v                                     ||
  +----------------------------------------------+          ||
  |            Stream Protocol                   |          ||                  Structured
  +----------------------------------------------+          ||                    Streams
        +^                       +^                         ||
        || Channels              ||                         ||
        v+                       v+                         v+
  +-------------------+    +---------------------+    +---------------------+
  |      Channel      |+-->|    Negotiation      |+-->|   Registration      |
  |      Protocol     |<--+|    Protocol         |<--+|   Protocol          |
  +-------------------+    +---------------------+    +---------------------+
        +^                       +^                         +^
  ------||-----------------------||-------------------------||----------------------------
        v+                       v+                         v+
  +-------------------------------------------------------------------------+
  |            Socket Protocol (UDP, IP, link layer)                        |
  +-------------------------------------------------------------------------+
```

Application Protocol: data streams
  - long term keys
  - `ssu::host`

Stream Protocol:
  - sending mux/demux (multiple app streams with priorities)
  - data retransmission and congestion control
  - distinguish real-time and background data
  - special streams for datagrams (dg stream, audio stream, video stream)
  - `ssu::stream`

Channel Protocol: curvecp-like
  - short term keys
  - packet end-to-end encryption
  - forward secrecy
  - `ssu::channel`

Pluggable congestion control: e.g. LEDBAT for file sync, Chicago for active sessions etc.
  - `ssu::decongestion`

Socket Protocol level:
  - receive datagrams and demux them to channels
  - `boost::asio::udp` (`uia::comm::socket`)

### 2.1 Interface Abstractions

#### 2.1.1 Sessions

A session represents a context in which SSU runs over some underlying network protocol such as UDP or IP. Each session represents an association between two network endpoints. A session is always uniquely defined by a pair of endpoints, but the definition of an endpoint depends on the underlying protocol on which SSU runs:

 * When SSU is run atop UDP, an endpoint consists of an IP address paired with a 16-bit UDP port number. From the perspective of any given host a session is thus uniquely defined by the 4-tuple (local IP, local port, remote IP, remote port). The session tuple for the opposite host is obtained by swapping the local and remote parts of the tuple.

 * If SSU is run directly atop IP as a "native" transport alongside TCP and UDP, then an endpoint consists only of an IP address, and thus an SSU session is uniquely defined by the pair of IP addresses of the hosts involved: (local IP, remote IP). By definition there can be only one such "native" SST session at a time between any pair of hosts.

 * If SSU is run atop some other network- or link-layer protocol, then SSU uses as its "endpoints"whatever the underlying protocols uses as an "address" or "host identifier." If SSU were to be run directly atop Ethernet, for example, then SSU’s endpoints would be IEEE MAC addresses, and a session would be uniquely defined by a pair of MAC addresses.

#### 2.1.2 Channels

The channel abstraction provides the interface between the channel protocol and the stream protocol. The channel protocol can multiplex arbitrary number of channels onto a session. The number of channels is only limited by the machine's available memory. Channel identifiers are 32-byte short-term public keys of the peer; thus, a channel is uniquely identified by the 4-tuple of (local endpoint, local channel, remote endpoint, remote channel). Each channel represents a separate instance of the SSU channel protocol resulting from a successful key exchange and feature negotiation using the negotiation protocol; SSU’s channels are therefore analogous in function to security associations in IPsec. Different channels always use independent key pairs for encryption and authentication. A given channel always uses one set of key pairs and negotiated parameters, however: when SSU needs to re-key a communication session (e.g., to ensure freshness of keys), it does so by creating a new channel through a fresh run of the negotiation protocol and terminating use of the old channel.

In SSU both parties may be trying to establish connection to each other. This may lead to two completely separate channels being set up. If this happens and peers detect it, they SHOULD migrate their streams toward the more recent channel. However, they MAY decide to not do it for increased security or logical packet separation.

Since channels rotate rather frequently (once in about 30 minutes), streams may be grouped differently onto each new set of channels, for example if a channel is set up and another channel expires, its streams may be reattached onto already existing channel.

The initial flow in a zero-RTT setup is potentially less forward secure than possible so we should assume that the connection will upgrade to a truly ephemeral key for subsequent flows on the same connection.
^^ it is not if client already supplies its ephemeral public key with the first packet?

We should provide support to pad packets to reduce vulnerability to traffic analysis. Both size and frequency of the encrypted packets should be adjustable.

Datagrams are specified to always fit in the smallest IPv6 datagram, 1280 bytes. (plus a typical 20 byte IPv4 or 40 byte IPv6 and 8 byte UDP header)


#### 2.1.3 Streams

We expect that different streams will have distinct transport characteristics which may be set or modified by the application. These include such distinct characteristic settings as:
 * Adjustable redundancy levels (trade bandwidth for latency savings)
 * Adjustable prioritization levels.

We expect that some control channel, which may be viewed as an out-of-band stream, will always be available and may be used to signal state changes for the rest of the streams. The control channel will likely consist of special purpose frames (control frames).

Streams are partitioned into frames for placement into channel packets. Whenever possible, any particular packet's payload should come from only one stream. Such dedication will reduce the probability that a single packet loss will impede progress of more than one stream. When there is not sufficient data in a stream to fill a packet, then frames from more than one stream may be packed into a single packet. Such packing should reduce the packet-count-overhead, and diminish serialization latency.

#### 2.1.4 Application interface

To better support an efficient and tight binding with an application, the following current statistics are plausibly expected to be made visible to the application:
  1. RTT (current smoothed estimate)
  2. Packet size (including all overhead;; also excluding overhead, only including payload)
  3. Bandwidth (current smoothed estimate across of entire connection)
  4. Peak Sustained Bandwidth (across entire connection)
  5. Congestion window size (expressed in packets)
  6. Queue size (packets that have been formed, but not yet emitted over a wire)
  7. Bytes in queue
  8. Per-stream queue size (either bytes per stream, or unsent packets, both??)

Notification should also be provided, or access for the following events [granularity of notification is TBD, and there should be no requirement on timeliness of the notifications, but any notification or status should include a best estimate of when the actual event took place]:
  1. Queue size has dropped to zero
  2. All required ACKs have been received (connection may be closed with no transmission state loss.)
    a. ACK of specific packet (section of stream?) has been received (not all streams support this. [Should this be queryable, rather than a notification??])

### 2.2 Packet types
```
packet
+--negotiation
   +--HELLO
   +--COOKIE
   +--INITIATE
+--message
   +--data
      +--frame sequence
   +--FEC
      +--redundant data for single-packet recovery
```

## 3 The Negotiation Protocol

SSU’s negotiation protocol is responsible for setting up new channels for use by the channel and stream protocols. The negotiation protocol is responsible for performing short-term key agreement and host identity verification.

### 3.1 Basic Design Principles

The negotiation protocol is asymmetric in that the two participants have clearly delineated "initiator" and "responder" roles. The protocol supports peer-to-peer as well as client/server styles of communication, however, and the channels resulting from negotiation are symmetric and can be used by either endpoint to initiate new logical streams to the other endpoint.

The INITIATE packet from the initiator must contain proper responder cookie or be discarded. The more general technique of "remote storage" eliminates storage on a responder in favor of storage inside the network: the responder sends data as an encrypted authenticated message to itself via the initiator inside an opaque cookie.

#### 3.1.1 Anti-amplification Measures

First Initiator message - HELLO packet - must be bigger from the connecting initiator than the responder COOKIE reply to reduce amplification attack possibility.

The responder doesn't retransmit its first packet, the COOKIE packet. The initiator is responsible for repeating its HELLO packet to ask for another COOKIE packet.

#### 3.1.2 Replay Attacks Protection

If the attacker makes copies of a legitimate initiator's HELLO packets then the attacker will receive responder COOKIE packets without affecting the responder state; these COOKIE packets do not leak information and will be rejected by the legitimate initiator. If the attacker makes copies of other initiator packets then the copies will be rejected by this responder and by other responders. If the attacker makes copies of responder packets then the copies will be rejected by this initiator and by other initiators.

#### 3.1.3 Forward Secrecy

Protocol provides forward secrecy for the initiator's long-term public key. Two minutes after a connection is closed, the responder is unable to extract the initiator's _long-term public key_ from the network packets that were sent to that responder, and is unable to verify the initiator's long-term public key from the network packets -- that is because initiator is always using short-term public key to encrypt -- the only place where initiator's long-term public key is revealed is in Vouch subpacket, which is inside a crypto box.

Here's how the forward secrecy works. At the beginning of a connection, the Responder generates a short-term public key S' and short-term secret key s', supplementing its long-term public key S and long-term secret key s. Similarly, the Initiator generates its own short-term public key C' and short-term secret key c', supplementing its long-term public key C and long-term secret key c. Almost all components of packets are in cryptographic boxes that can be opened only by the short-term secret keys s' and c'. The only exceptions are as follows:

 * Packets from the Initiator contain, unencrypted, the short-term public key C'. This public key is generated randomly for this connection; it is tied to the connection but does not leak any other information.
 * The first packet from the Initiator contains a cryptographic box that can be opened by __c' and by s__ (not s'; the initiator does not know S' at this point). This box contains Initiator's long-term public key C for validation against black-list by the Responder.
 * The first packet from the Responder contains a cryptographic box that can be opened by __c' and by s__. However, this box contains nothing other than the Responder's short-term public key S', which is generated randomly for this connection, and a cookie, discussed below.
 * The second packet from the Initiator contains a cookie from the Responder. This cookie is actually a cryptographic box that can be understood only by a "minute key" in the Responder. Two minutes later the Responder has discarded this key and is unable to extract any information from the cookie.
 * At the end of the connection, both sides throw away the short-term secret keys s' and c'.

Channel holds short-term keys for encryption session. Closing a channel destroys those keys, providing forward secrecy.

Channels are closed after arbitrary amount of time to flush keys.

#### 3.1.4 Security requirements for nonces

Attached to the box is a public 24-byte nonce chosen by the sender. Nonce means "number used once." After a particular nonce has been used to encrypt a packet, the same nonce must never be used to encrypt another packet from the sender's secret key to this receiver's public key, and the same nonce must never be used to encrypt another packet from the receiver's secret key to this sender's public key. This requirement is essential for cryptographic security.

### 3.2 Negotiation Protocol Packet Format

Negotiation protocol uses 3-way message exchange to verify peer's identity and start secure channel. Actual data transfer may start already with the third packet. A faster Zero-RTT connection establishment is not used to provide better forward secrecy guarantees.

#### 3.2.1 Responder Cookie format

```
16 bytes: compressed nonce, prefix with "minute-k"
80 bytes: secretbox under minute-key, containing:
    32 bytes: initiator short-term public key
    32 bytes: responder short-term secret key
TOTAL: 96 bytes
```

When the minute key expires, the cookie could not be decrypted and will make connection attempts with this cookie ignored.

#### 3.2.2 HELLO packet format

First packet sent by the Initiator willing to establish a connection. This packet is artificially padded with zeros to make it larger than the response packet, reducing amplification attacks possibility.

```
0   : 8  : magic
8   : 32 : initiator short-term public key
40  : 64 : zero
104 : 8  : compressed nonce
112 : 80 : box C'->S containing:
            0  : 32 : initiator long-term public key (for pre-auth)
            32 : 32 : zero
TOTAL: 192 bytes
```

#### 3.2.3 COOKIE packet format

In response to HELLO packet, the Responder does not allocate any state. Instead, it encodes information about the Initiator into the returned Cookie. If the Initiator is willing to continue session it responds with an Initiate packet, which may contain initial message data along with identifying Cookie.

In response, Responder encodes initiator's short-term public key and own short-term secret key using a special minute key, which is rotated every minute. If session isn't started within this interval, the responder will not be able to open this box and will discard the Initiate packet, thus failing session negotiation.

```
0  : 8   : magic
8  : 16  : compressed nonce
24 : 144 : box S->C' containing:
            0  : 32 : responder short-term public key
            Responder Cookie:
            32 : 16 : compressed nonce
            48 : 80 : minute-key secretbox containing:
                       0  : 32 : initiator short-term public key
                       32 : 32 : responder short-term secret key
TOTAL: 168 bytes
```

#### 3.2.4 INITIATE packet format

When Initiate packet is accepted, starting a session, cookie must be placed into a cache and cleaned when minute key is rotated to avoid replay attacks.

@todo Require a congestion control negotiation frame inside INITIATE message before sending any stream data.

```
0   : 8     : magic
8   : 32    : initiator short-term public key
40  : 96    : responder's cookie
               0  : 16 : compressed nonce
               16 : 80 : minute-key secretbox containing:
                          0  : 32 : initiator short-term public key
                          32 : 32 : responder short-term secret key
136 : 8     : compressed nonce
144 : 112+M : box C'->S' containing:
144 :          0   : 32  : initiator long-term public key
176 :          32  : 16  : compressed nonce
192 :          48  : 48  : box C->S containing Vouch subpacket:
                            0 : 32 : initiator short-term public key
240 :          96  : M   : message
TOTAL: 240+M+16 bytes
```

M size is in multiples of 16 between 16 and 1024 bytes.

#### 3.2.5 INITIATOR MESSAGE packet format

Responder and Initiator message packets differ only in the kind of short term key and direction of encryption of the box. Each side sends packets with their own short-term public key as identifier.

```
0   : 8    : magic
8   : 32   : initiator short-term public key C'
40  : 8    : compressed nonce
48  : 16+M : box C'->S' containing:
              0 : M : message
TOTAL: 64+M bytes
```

M size is in multiples of 16 between 48 and 1088 bytes.

#### 3.2.6 RESPONDER MESSAGE packet format

```
0  : 8    : magic
8  : 32   : responder short-term public key S'
40 : 8    : compressed nonce
48 : 16+M : box S'->C' containing:
             0 : M : message
TOTAL: 64+M bytes
```

M size is in multiples of 16 between 48 and 1088 bytes.

## 4 Channel Protocol

Channel protocol provides independently encrypted packetization for streams of data. Channel protocol multiplexes streams, provides packet acknowledgement, controls congestion and provides
out-of-band signaling for streams management.

Figure 2: Channel protocol packet layout [source](http://www.asciidraw.com/#608745302879820887/460823121)
```
              31                        16 15                      0
          +-   +--------------------------+------------------------+
      UDP |    |        Source Port       |    Destination Port    |
   Header |    +--------------------------+------------------------+
  8 bytes |    |          Length          |       Checksum         |
          +-   +--------------------------+------------------------+

          +-   +---------------------------------------------------+
          |    |                                                   |
          |    +              Packet magic (8 bytes)               +
          |    |                                                   |
          |    +---------------------------------------------------+
  Channel |    |                                                   |
   Header |    +                                                   +
 48 bytes |    |               Short-term public key               |
          |    +                                                   +
          |    |                     (32 bytes)                    |
          |    +                                                   +
          |    |                                                   |
          |    +---------------------------------------------------+
          |    |                                                   |
          |    +             Compressed nonce (8 bytes)            +
          |    |                                                   |
          +-   +---------------------------------------------------+
               |                                                   |
               |                Message in crypto box              |
                                       ......
               |                                                   |
               +---------------------------------------------------+
```

As a final step in session negotiation channel layer sets up a decongestion strategy.

### 4.1 MESSAGE box format

After decrypting, we will have a plaintext payload block.

A non-FEC packet is a data packet and consists of one or more frames. Each frame has it's type as first byte. Each frame type is described below. A FEC packet consists of a XOR of all zero-padded packets in this FEC group and can be used to recover one lost packet in this FEC group.

Non-FEC packet payload consists of:
 * A packet header (see 4.1.1)
 * One (Zero?) or more tagged frames (see 4.2)
 * Zero-padding. This padding produces a total message length that is a multiple of 16 bytes, at least 16 bytes and at most 1088 bytes.

#### 4.1.1 Packet header format

   * Flags
     * Sizes of optional packet header fields
     * Optimistic ACK entropy bit
     * Last FEC group packet bit
   * Protocol version number (variable size)
   * Packet sequence number (variable size)
   * FEC group number (optional?)
   * Packet ACKs?

### 4.2 Framing

Frames are stream containers within a channel packet. Packet contents are sliced into frames, which may contain stream data from one or more streams and other control information. Frames use tagged chunks format, where each chunk follows a certain format with a header and optionally content part.

Frames are inside the channel message cryptobox, prevented from peeking into by any eavesdroppers.

#### 4.2.1 Frame types
```
+----------------------+------------------------+
|     Type value       |     Frame type         |
+----------------------+------------------------+
|      fioood00        |  STREAM                |
|      xxxx0001        |  ACK                   |
|      xxxx0011        |  PADDING               |
|      xxxx0101        |  DECONGESTION          |
|      xxxx0111        |  DETACH                |
|      xxxx1001        |  RESET                 |
|      xxxx1011        |  CLOSE                 |
+----------------------+------------------------+
```

#### 4.2.2 STREAM frame

Stream frame is used to transfer data on each individual stream. It also serves as an ATTACH packet
to initiate a new stream.

Frame type: 0
Flags: FIN, INIT, OFFSET, DATA LENGTH

When INIT bit is set, this frame initiates the stream by providing stream and parent unique IDs.
When FIN bit is set, this frame marks last transmission on this stream in this direction.
OFFSET bits encode length of the stream offset field.
When DATA LENGTH bit is set, this frame has a limited number of bytes for this stream, provided in
length field, otherwise stream data occupies the rest of the packet.

#### 4.2.3 ACK frame

#### 4.2.4 PADDING frame

Padding frame indicates padding data within a packet, used to counter traffic analysis attacks. Ideally all packets should be padded to have same final length of 1280 bytes. However, in case of slow connection it may be preferential to not pad the data packets or pad them to a shorter common length.
Padding frame contents may be anything and are simply ignored. It is recommended to explicitly initialize this padding data area with either random or zero bytes and not leave any old data that might leak information.

```
      0         1         2         3                 3+L-1
 +---------+---------+---------+---------+  .....  +---------+
 | Type(3) | Length of padding |        Padding data         |
 +---------+---------+---------+---------+  .....  +---------+
```
Padding packet may be as small as 3 bytes - type byte and length field, which is zero in this case.

Take note of the rounding requirements - all padding together must pad message size to an integer multiple of 16 bytes.

#### 4.2.5 DECONGESTION frame

Decongestion feedback frame contents are specific to chosen decongestion method in the channel.
Frame format for several implemented methods will be listed here.

```
      0         1         2
 +---------+---------+---------+  .....  +---------+
 | Type(5) | Subtype | Method specific contents    |
 +---------+---------+---------+  .....  +---------+
```

The following diagrams show method specific contents section starting from subtype byte, type is the same for all these packets and is omitted for brevity.

##### 4.2.5.1 Congestion control feedback for TCP Cubic

Similar to TCP protocol, packet loss and receive window size are provided.

```
      0         1         2         3         4
 +---------+---------+---------+---------+---------+
 | Subt(1) | Num lost packets  |   Receive window  |
 +---------+---------+---------+---------+---------+
```
 * Subt `uint8_t`: The congestion control subtype (1 for TCP Cubic)
 * Num lost packets `big_uint16_t`: The number of packets lost over the lifetime of this connection. This may wrap for long-lived connections.
 * Receive window `big_uint16_t`: The TCP receive window.

##### 4.2.5.2 Congestion control feedback for CurveCP Chicago
##### 4.2.5.3 Congestion control feedback for UDP LEDBAT
##### 4.2.5.4 Congestion control feedback for WebRTC Inter-arrival

```
          0         1         2         3         4         5         6         7
     +---------+---------+---------+---------+---------+---------+---------+---------+
  +0 | Subt(4) | Num lost packets  | Received|                     Smallest Received |
     +---------+---------+---------+---------+---------+---------+---------+---------+
  +8 | Packet            |                              Smallest Delta Time          |
     +---------+---------+---------+---------+---------+---------+---------+---------+
 +16 |                   |   Packet Delta    |           Packet Time Delta           |
     +---------+---------+---------+---------+---------+---------+---------+---------+
```
 * Subt `uint8_t`: The congestion control subtype (4 for Inter-arrival)
 * Num lost packets `big_uint16_t`: The number of packets lost over the lifetime of this connection. This may wrap for long-lived connections.
 * Received `uint8_t`: An 8 bit unsigned value specifying the number of received packets in this update.
 * Smallest Received Packet: The lower 48 bits of the smallest sequence number represented in this update.
 * Smallest Delta Time `big_uint64_t`: A 64 bit unsigned value specifying the delta time from connection creation when the above packet was received.
 * Packet Delta `big_uint16_t`: A 16 bit unsigned value specifying the sequence number delta from the smallest received. Always followed immediately by a corresponding Packet Time Delta.
 * Packet Time Delta `big_uint32_t`: A 32 bit unsigned value specifying the time delta from smallest time when the preceding packet sequence number was received.

#### 4.2.6 DETACH frame

Detach frame allows stream to detach from current channel without shutting down the stream.

#### 4.2.7 RESET frame

Abort stream. (Might combine detach and reset frames into STOP frame!)

#### 4.2.8 CLOSE frame

Close connection.

## 5 Stream Protocol

Streams are independent sequences of uni- or bi­directional data flows cut into stream frames. Streams group logically communications between two parties. Streams can be created by either peer; can concurrently send data interleaved with other streams; and can be cancelled.

Streams must be attached to channels to be able to send. Streams that need to send data attach onto a channel based on their integer priority. Stream with absolute largest priority on the channel wins and will always send first as long as it has data to send.

Streams have a unique ID, used to distinguish this stream after switching to a newly established channel. Streams in channels keep their global IDs and continue delivering data even after channel switch.

> Old CurveCP description:

A byte stream is a string of bytes, between 0 bytes and 2^60-1 bytes (allowing more than 200 gigabits per second continuously for a year), followed by either success or failure. The bytes in an N-byte stream have positions 0,1,2,...,N-1; the success/failure bit has position N. A message from the sender can include a block of bytes of data from anywhere in the stream; a block can include as many as 1024 bytes. A message from the receiver can acknowledge one or more ranges of data that have been successfully received.

The first range acknowledged in a message always begins with position 0. Subsequent ranges acknowledged in the same message are limited to 65535 bytes. Each range boundary sent by the receiver matches a boundary of a block previously sent by the sender, but a range normally includes many blocks.

Once the receiver has acknowledged a range of bytes, the receiver is taking responsibility for all of those bytes; the sender is expected to discard those bytes and never send them again. The sender can send the bytes again; usually this occurs because the first acknowledgment was lost. The receiver discards the redundant bytes and generates a new acknowledgment covering those bytes.

### 5.1 Initiating root stream (LSID 0)

Root stream does not have a unique ID and therefore is always implicitly started on a channel. It is used to spawn first application-level stream and for out-of-band signaling about other streams.

### 5.2 Starting new stream.

New stream is started by posting STREAM frame with INIT flag set.

### 5.3 Initiating sub-streams

### 5.4 Attaching a stream to channel

### 5.5 Detaching a stream from channel

### 5.6 Stream data exchange

Once a stream is created, it can be used to send arbitrary amounts of data. Generally this means that a series of frames will be sent on the stream until a frame containing the FIN bit is sent. Once the FIN has been sent, the stream is considered to be half­-closed.

### 5.7 Stream half­-close

When one side of the stream sends a frame with FIN set to true, the stream is considered to be half-­closed from that side. The sender of the FIN is indicating that no further data will be sent from the sender on this stream. When both sides have half­-closed, the stream is considered to be closed.

### 5.8 Stream close

When both sides have indicated their desire to stop sending on the stream, stream becomes closed.

## 6. References

 * [RDP Reliable Data Protocol](https://tools.ietf.org/html/rfc908)
 * [ECN in IP](https://tools.ietf.org/html/rfc3168)
 * [TCP extensions for highperf](https://tools.ietf.org/html/rfc1323)
 * [SPDY protocol](http://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3-1)
