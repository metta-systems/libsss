Structured Secure Streams
=========================

SSS provides secure encrypted and authenticated data connection between endpoints. In the future SSS will also provide routing services for the so-called [Unmanaged Internet Architecture (UIA)](http://pdos.csail.mit.edu/uia/).

Builds as part of [mettanode](https://github.com/berkus/mettanode), or standalone with [libarsenal](https://github.com/berkus/libarsenal) dependency.

### Introduction

SSS is based on experimental, unfinished project under UIA - [Structured Streams Transport](http://pdos.csail.mit.edu/uia/sst/).

SSS is an experimental transport protocol designed to address the needs of modern applications
that need to juggle many asynchronous communication activities in parallel, such as downloading
different parts of a web page simultaneously and playing multiple audio and video streams at once.

Features of SSS:

 * Multiplexes many application streams onto one network connection
 * Gives streams hereditary structure: applications can spawn lightweight streams from existing ones
   * Efficient: no 3-way handshake on startup or TIME-WAIT on close
   * Supports request/response transactions without serializing onto one stream
   * General out-of-band signaling: control requests already in progress
 * Both reliable and best-effort delivery in a semantically unified model
   * supports messages/datagrams of any size: no need to limit size of video frames, RPC responses, etc.
 * Dynamic prioritization of application's streams
   * e.g., load visible parts of a web page first, change priorities when user scrolls
 * End-to-end cryptographic security comparable to SSL
 * Peer-to-peer communication across NATs via hole punching
 * Implemented as a library that can be linked directly into applications like SSL for easy deployment
 * Written in standard c++14 with boost.

![streams](https://raw.github.com/berkus/libsss/master/doc/streams.png "Streams Structure")

### Structures

`sss::host` holds information about this host's connection sessions.

`sss::server` provides access to services on this `sss::host`.

`sss::stream` provides outgoing connection from this `sss::host`.

There can be multiple servers and streams connected to multiple endpoints.

[![Build Status](https://travis-ci.org/berkus/libsss.png?branch=master)](https://travis-ci.org/berkus/libsss) [![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/berkus/libsss/trend.png)](https://bitdeli.com/free "Bitdeli Badge")
