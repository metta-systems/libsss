-8<-packet->8- frame types
```
invalid  = 0x0, ///< Always invalid
init     = 0x1, ///< Initiate new stream STREAM
reply    = 0x2, ///< Reply to new stream STREAM?
data     = 0x3, ///< Regular data packet STREAM
datagram = 0x4, ///< Best-effort datagram STREAM?
ack      = 0x5, ///< Explicit acknowledgment ACK
reset    = 0x6, ///< Reset stream RESET
attach   = 0x7, ///< Attach stream STREAM
detach   = 0x8, ///< Detach stream DETACH
```

Frames in QUIC:
```
stream              ///< Initiate new stream, Attach stream, Regular data packet STREAM
ack                 ///< Explicit acknowledgment ACK
congestion_control  ///< Congestion parameters from the other side DECONGESTION
reset_stream        ///< Reset stream RESET
connection_close    ///< Detach stream(s) and close whole connection CLOSE
goaway_stream       ///< Stopped listening ??
```

service request codes
```
connect_request        = 0x101, ///< Connect to named service.
// request format: string service, string protocol
connect_reply          = 0x201, ///< Response to connect request.
// reply format: reply code, string description
list_services_request  = 0x102, ///< Spec 4.4 end
list_services_reply    = 0x202, ///< with human-readable descriptions
/// reply format: array of pairs <service, service_desc>
list_protocols_request = 0x103, ///< List protocols for given service
/// request format: string service_name
list_protocols_reply   = 0x203, ///< with descriptions
/// reply format: array of pairs <protocol, protocol_desc>

reply_ok         = 0,        ///< Service request accepted.
reply_not_found  = 1,        ///< Specified service pair not found.
```

http://codesinchaos.wordpress.com/2012/09/09/curvecp-1/ comments on protocol security
^^ READ IT AGAIN ^^

```
         +-------------------------------------------------------------+
         |  Link                                                       |
         | +---------------------------------------------------------+ |
         | |Channel 0: control messages                              | |
         | +---------------------------------------------------------+ |
         | +---------------------------------------------------------+ |
         | |Channel 1                                                | | Streams must be attached
         | |+----+  +----------------------------------------------+ | | to channels to be able
         | ||    |  | Channel Root: Stream LSID 0                  | | | to send.
         | ||Auth|  ++---------------------------------------------+ | | Up to 255 channels per
         | ||    |   | +-------------------------------------------+ | | session.
         | ||    |   +>|Service Request Stream (service:protocol)  | | |
Endpoint | ||    |     +-------------------------------------------+ | | Endpoint
10.0.0.1 | ||    |     +-------------------------------------------+ | | 10.0.0.2
+------->| ||    |     |  Stream USID (Application Root)           | | +-------->
         | ||    |     +-------------------------------------------+ | |
         | ||    |     +-------------------------------------------+ | |
         | ||    |     |  Stream USID                              | | |
         | ||    |     +-------------------------------------------+ | |
         | ||    |     +-------------------------------------------+ | |
         | ||    |     |  Stream USID                              | | |
         | |+----+     +-------------------------------------------+ | |
         | +---------------------------------------------------------+ |
         | +---------------------------------------------------------+ |
         | |Channel N (2-255)                                        | |
         | +---------------------------------------------------------+ |
         +-------------------------------------------------------------+
```

base_stream::tx_waiting_ack_ must be reviewed:
- there can be at most five unaccepted ranges in the stream
- if new packet creates more unaccepted ranges - silently drop it
- there may be more unaccepted ranges in the local send/recv state
- overlapping ranges need range ariths (covering and merging range holes)

datagram_stream
+--data_stream (adds retransmission and congestion control strategy)
+--audio_stream
+--video_stream
