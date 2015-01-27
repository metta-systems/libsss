//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/asio/io_service.hpp>

namespace sss {

class host;

/**
 * Host state incapsulating asio run loop and possibly other related state.
 */
class asio_host_state
{
protected:
    /**
     * I/O service that needs to be run in order to service protocol interactions.
     */
    boost::asio::io_service io_service_;

public:
    void run_io_service();
    inline boost::asio::io_service& get_io_service() { return io_service_; }

    virtual std::shared_ptr<host> get_host() = 0;
};

} // sss namespace
