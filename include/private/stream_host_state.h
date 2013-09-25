//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <unordered_map>
#include <utility>
#include <string>
#include "hash_combine.h"
#include "algorithm.h"

// Hash specialization for pair<string,string>
// Need to have it before declaration of listeners_ below.
namespace std {

template<>
struct hash<std::pair<std::string, std::string>> : public std::unary_function<std::pair<std::string, std::string>, size_t>
{
    inline size_t operator()(std::pair<std::string, std::string> const& a) const noexcept
    {
        // VEEERY bad implementation for now. @fixme
        size_t seed = 0xdeadbeef;
        stdext::hash_combine(seed, a.first);
        stdext::hash_combine(seed, a.second);
        return seed;
    }
};

} // namespace std

namespace ssu {

class stream_responder;
class stream_peer;
class server;

/**
 * Host state related to stream management.
 * @fixme This whole class must be protected - not part of public host API.
 */
class stream_host_state
{
    stream_responder* responder_{nullptr};
    std::unordered_map<peer_id, stream_peer*> peers_;
    std::unordered_map<std::pair<std::string, std::string>, server*> listeners_;

public:
    inline stream_host_state() = default;
    virtual ~stream_host_state();

    virtual std::shared_ptr<host> get_host() = 0;

    /**
     * We instantiate stream responder when listening for incoming connections in ssu::server,
     * or when setting up peer key exchange in stream_peer.
     * @fixme just create the responder right away and don't bother? It's low footprint.
     */
    void instantiate_stream_responder();

    /**
     * Create if necessary and return the stream peer's information (from the other side).
     */
    class stream_peer* stream_peer(peer_id const& id);
    /**
     * Return the stream peer's information (from the other side) or nullptr.
     */
    class stream_peer* stream_peer_if_exists(peer_id const& id);

    inline bool is_listening(std::pair<std::string, std::string> svc_pair) const {
        return contains(listeners_, svc_pair);
    }

    inline void register_listener(std::pair<std::string, std::string> svc_pair, server* srv) {
        listeners_.insert(make_pair(svc_pair, srv));
    }
    // void unregister_listener()

    inline server* listener_for(std::string service, std::string protocol) {
        if (!contains(listeners_, make_pair(service, protocol)))
            return nullptr;
        return listeners_[make_pair(service, protocol)];
    }
};

} // ssu namespace
