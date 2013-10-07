//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/asio/io_service.hpp>

namespace ssu {

class host;

/**
 * Host state incapsulating asio run loop and possibly other related variables.
 */
class asio_host_state
{
protected:
	/**
	 * I/O service that needs to be run in order to service protocol interactions.
	 */
	boost::asio::io_service io_service;

public:
	inline void run_io_service() { io_service.run(); }
	inline boost::asio::io_service& get_io_service() { return io_service; }

    virtual std::shared_ptr<host> get_host() = 0;
};

} // namespace ssu
