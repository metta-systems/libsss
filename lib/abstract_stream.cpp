//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "abstract_stream.h"

namespace ssu {

abstract_stream::abstract_stream(std::shared_ptr<host> h)
    : host_(h)
{}

void abstract_stream::set_priority(int priority)
{
    priority_ = priority;
}

} // ssu namespace
