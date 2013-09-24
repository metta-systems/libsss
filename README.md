Structured Streams Unleashed
============================

SSU provides secure encrypted and authenticated data connection between endpoints. In the future SSU will also provide routing services for the so-called [Unmanaged Internet Architecture (UIA)][1].

Builds as part of [mettanode][2], or standalone with [libsupport][3] dependency.

### Structures

`ssu::host` holds information about this host's connection sessions.

`ssu::server` provides access to services on this `ssu::host`.

`ssu::stream` provides outgoing connection from this `ssu::host`.

There can be multiple servers and streams connected to multiple endpoints.

  [1]: http://pdos.csail.mit.edu/uia/ "UIA"
  [2]: https://github.com/berkus/mettanode "MettaNode"
  [3]: https://github.com/berkus/libsupport "support"
