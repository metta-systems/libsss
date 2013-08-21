//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/asio/deadline_timer.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/signals2/signal.hpp>
#include "asio_host_state.h"
#include "timer_engine.h"

namespace ssu {

class timer_host_state;

namespace async {

/**
 * Interval timer supporting usual packet resend timeouts and such.
 */
class timer
{
public:
	typedef boost::asio::deadline_timer::duration_type duration_type;

	static const duration_type retry_min;
	static const duration_type retry_max;
	static const duration_type fail_max;

private:
	std::unique_ptr<timer_engine> engine_{nullptr};
	duration_type interval_{retry_min};
	boost::asio::deadline_timer::time_type deadline_;
	bool active_{false};

public:
	timer(ssu::timer_host_state* host);

	void start(duration_type interval); // @todo: support deadline timers too
	void stop();
	void restart();
	bool has_failed() const;

	inline bool is_active() const {
		return active_;
	}
	inline duration_type interval() const {
		return interval_;
	}

	/**
	 * Signaled when the timer expires.
	 *
	 * timeout signal handler signature is
	 * void timeout(bool failed)
	 * 
	 * Argument 'failed' is true if the hard failure deadline has been reached.
	 */
    typedef boost::signals2::signal<void (bool)> timeout_signal;
    timeout_signal on_timeout;
};

} // namespace async

class timer_host_state : virtual public asio_host_state
{
public:
	/**
	 * Get current time. Can be virtualized to provide simulated time in testing environments.
	 * @return Current time in a boost.asio-defined time type.
	 */
	virtual boost::posix_time::ptime current_time();
	/**
	 * Create a timer engine to perform various delays. Can be virtualized to simulate delays.
	 * @param  t Timer to attach the engine to.
	 * @return   New copy of a timer engine.
	 */
	virtual std::unique_ptr<async::timer_engine> create_timer_engine_for(async::timer* t);
};

} // namespace ssu
