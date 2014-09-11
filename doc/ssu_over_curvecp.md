packet types
```
invalid  = 0x0, ///< Always invalid
init     = 0x1, ///< Initiate new stream
reply    = 0x2, ///< Reply to new stream
data     = 0x3, ///< Regular data packet
datagram = 0x4, ///< Best-effort datagram
ack      = 0x5, ///< Explicit acknowledgment
reset    = 0x6, ///< Reset stream
attach   = 0x7, ///< Attach stream
detach   = 0x8, ///< Detach stream
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
         | ||    |     +-------------------------------------------+ | | +-+-----------------------+
         | ||    |     |  Stream USID                              | | | | | Feedback (ACKs)       |
         | ||    |     +-------------------------------------------+ | | | +-----------------------+
         | ||    |     +-------------------------------------------+ | | | | Privacy (en-/decrypt) |
         | ||    |     |  Stream USID                              | | | | +-----------------------+
         | |+----+     +-------------------------------------------+ | | | | Integrity (MAC)       |
         | +---------------------------------------------------------+ | | +-----------------------+
         | +---------------------------------------------------------+ | | | Encoding (FEC)        |
         | |Channel N (2-255)                                        | | | +-----------------------+
         | +---------------------------------------------------------+ | |   Sequencing (TSNs)     |
         +-------------------------------------------------------------+ +-------------------------+
```
