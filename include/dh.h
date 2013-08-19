#pragma once

#include "byte_array.h"
#include "negotiation/key_message.h"

namespace ssu {
namespace negotiation {

class dh_hostkey_t
{
public:
    byte_array public_key;
    byte_array hkr; // hmac secret key

    //temporary for testing
    dh_hostkey_t() { public_key.resize(256); hkr.resize(32); }
};

} // namespace negotiation

class dh_host_state
{
public:
    std::shared_ptr<negotiation::dh_hostkey_t> get_dh_key(negotiation::dh_group_type group) {
        return std::make_shared<negotiation::dh_hostkey_t>(); //@fixme temporary
    }
};

} // namespace ssu
