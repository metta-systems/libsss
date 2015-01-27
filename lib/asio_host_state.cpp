//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/internal/asio_host_state.h"
#include <boost/thread.hpp>
#include <boost/bind.hpp>

using namespace boost;

namespace sss {

void asio_host_state::run_io_service()
{
    thread_group group;
    for (unsigned i = 0; i < thread::hardware_concurrency(); ++i) {
        group.create_thread(boost::bind(&asio::io_service::run, ref(io_service_)));
    }
    group.join_all();
}

} // sss namespace
