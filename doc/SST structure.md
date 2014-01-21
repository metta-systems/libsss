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
[source diagram](http://www.asciidraw.com/#6509827088975172190/1377497774)

Setting up a channel.
=====================

#### Key exchange

Key exchange initiation is sent on channel 0, control messages.

Key exchange is presently a two-step operation consisting of two pairs of material exchanges,
setting up a DH key and sharing session symmetric encryption key.

Once the key exchange is completed, sides set up an encrypted communication channel.

#### Channel acceptance

Channel will be spawned once both steps of key exchange are completed successfully. The key
exchange initiator acts as initiating channel side, the other side is the responder.

#### Attach stream for transmission.

Streams that need to send data attach onto a channel based on their integer priority. Stream with
absolute largest priority on the channel wins and will always send first as long
as it has data to send.



Starting new stream.
====================
#### Initiating root stream (LSID 0)
#### Initiating sub-streams

