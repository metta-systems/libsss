Spec for SSU
with requirements and protocol abilities

## 1 Introduction

Requirements:
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


[channels][http://www.asciidraw.com/#5074225503160675365/76617341]


One problem (at least with normal implementations) is that the application cannot access the packets coming after a lost packet until the retransmitted copy of the lost packet is received. This causes problems for real-time applications such as streaming media, real-time multiplayer games and voice over IP (VoIP) where it is generally more useful to get most of the data in a timely fashion than it is to get all of the data in order.

## 2 Design Overview

Figure 1: Protocol Architecture [source][http://www.asciidraw.com/#5612283093966789321/1974720956]
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


### 2.1 Interface Abstractions

#### 2.1.1 Sessions

A session represents a context in which SSU runs over some underlying network protocol such as UDP
or IP. Each session represents an association between two network endpoints. A session is always
uniquely defined by a pair of endpoints, but the definition of an endpoint depends on the underlying
protocol on which SSU runs:

 * When SSU is run atop UDP, an endpoint consists of an IP address paired with a 16-bit UDP port
number. From the perspective of any given host a session is thus uniquely defined by the 4-tuple
(local IP, local port, remote IP, remote port). The session tuple for the opposite host is
obtained by swapping the local and remote parts of the tuple.

 * If SSU is run directly atop IP as a "native" transport alongside TCP and UDP, then an endpoint
consists only of an IP address, and thus an SSU session is uniquely defined by the pair of IP
addresses of the hosts involved: (local IP, remote IP). By definition there can be only one such
"native" SST session at a time between any pair of hosts.

 * If SSU is run atop some other network- or link-layer protocol, then SSU uses as its "endpoints"
whatever the underlying protocols uses as an "address" or "host identifier." If SSU were to be run
directly atop Ethernet, for example, then SSU’s endpoints would be IEEE MAC addresses, and a session
would be uniquely defined by a pair of MAC addresses.

#### 2.1.2 Channels

The channel abstraction provides the interface between the channel protocol and the stream protocol.
The channel protocol can multiplex arbitrary number of channels onto a session. The number of channels
is only limited by the machine's available memory. Channel identifiers are 32-byte short-term public
keys of the peer; thus, a channel is uniquely identified by the 4-tuple of (local endpoint, local
channel, remote endpoint, remote channel). Each channel represents a separate instance of the SSU
channel protocol resulting from a successful key exchange and feature negotiation using the
negotiation protocol; SSU’s channels are therefore analogous in function to security associations
in IPsec. Different channels always use independent key pairs for encryption and authentication.
A given channel always uses one set of key pairs and negotiated parameters, however: when SSU needs
to re-key a communication session (e.g., to ensure freshness of keys), it does so by creating
a new channel through a fresh run of the negotiation protocol and terminating use of the old channel.

#### 2.1.3 Streams

## 3 Channel Protocol

Figure 3: Channel protocol packet layout [source][http://www.asciidraw.com/#608745302879820887/460823121]
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


## 4 Stream Protocol



## 5 The Negotiation Protocol

SSU’s negotiation protocol is responsible for setting up new channels for use by the channel and stream protocols. The negotiation protocol is responsible for performing short-term key agreement and host identity verification.

### 5.1 Basic Design Principles

The negotiation protocol is asymmetric in that the two participants have clearly delineated "initiator" and "responder" roles. The protocol supports peer-to-peer as well as client/server styles of communication, however, and the channels resulting from negotiation are symmetric and can be used by either endpoint to initiate new logical streams to the other endpoint.

