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
#### Attach stream for transmission.

Starting new stream.
====================
#### Initiating root stream (LSID 0)
#### Initiating sub-streams

