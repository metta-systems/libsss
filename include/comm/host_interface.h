//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace uia {
namespace comm {

class socket;
class packet_receiver;

// Interface used by socket layer to work with the host state.
// Must be implemented by real host implementation, for example the one in sss.
class comm_host_interface
{
public:
    // Interface used by socket to register itself on the host.
    virtual void activate_socket(socket*) = 0;
    virtual void deactivate_socket(socket*) = 0;

    // Interface to bind and lookup receivers based on packet magic value.
    // bind_receiver(magic::hello, kex_responder)
    // bind_receiver(magic::initiate, kex_responder)
    // bind_receiver(magic::cookie, kex_initiator)
    // bind_receiver(magic::message, message_receiver)
    virtual void bind_receiver(std::string, std::weak_ptr<packet_receiver>) = 0;
    virtual void unbind_receiver(std::string) = 0;
    virtual bool has_receiver_for(std::string) = 0;
    virtual std::weak_ptr<packet_receiver> receiver_for(std::string) = 0;
};

} // comm namespace
} // uia namespace
