Structured Streams Unleashed
============================

SSU provides secure encrypted and authenticated data connection between endpoints. In the future SSU will also provide routing services for the so-called [Unmanaged Internet Architecture (UIA)][1].

Builds as part of [mettanode][2], or standalone with [libsupport][3] dependency.

### Introduction

SSU is based on experimental, unfinished project under UIA - [SST][4].

SSU is an experimental transport protocol designed to address the needs of modern applications
that need to juggle many asynchronous communication activities in parallel, such as downloading
different parts of a web page simultaneously and playing multiple audio and video streams at once.

Features of SSU:

 * Multiplexes **many application streams** onto **one network connection**
 * Gives streams **hereditary structure**: applications can spawn **lightweight streams** from existing ones
   * **Efficient**: no 3-way handshake on startup or TIME-WAIT on close
   * Supports **request/response transactions** without serializing onto one stream
   * General **out-of-band signaling**: control requests already in progress
 * Both **reliable** and **best-effort delivery** in a **semantically unified model**
   * supports **messages/datagrams of any size**: no need to limit size of video frames, RPC responses, etc.
 * **Dynamic prioritization** of application's streams
   * e.g., load visible parts of a web page first, change priorities when user scrolls
 * **End-to-end cryptographic security** comparable to SSL
 * **Peer-to-peer communication across NATs** via hole punching
 * Implemented as a library that can be linked directly into applications like SSL for easy deployment

### Structures

`ssu::host` holds information about this host's connection sessions.

`ssu::server` provides access to services on this `ssu::host`.

`ssu::stream` provides outgoing connection from this `ssu::host`.

There can be multiple servers and streams connected to multiple endpoints.

  [1]: http://pdos.csail.mit.edu/uia/ "UIA"
  [2]: https://github.com/berkus/mettanode "MettaNode"
  [3]: https://github.com/berkus/libsupport "support"
  [4]: http://pdos.csail.mit.edu/uia/sst/ "SST"
