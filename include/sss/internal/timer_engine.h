//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace sss {
namespace async {

class timer;

/**
 * Timer engine provides a substitutable interface to create and arm timers.
 * A custom timer engine subclass is used for simulation purposes, allowing whole
 * protocol to run on simulated time.
 */
class timer_engine
{
	timer* origin_{0};
public:
	typedef boost::posix_time::time_duration duration_type;

	/**
	 * Create a new timer engine.
	 */
	timer_engine(timer* t) : origin_(t) {}

	/**
	 * Start the timer.
	 * The implementation subclass provides this method.
	 * @param interval the timer interval.
	 */
	virtual void start(duration_type interval) = 0;
	/**
	 * Stop the timer.
	 * The implementation subclass provides this method.
	 */
	virtual void stop() = 0;

	/**
	 * Signal timeout on the origin timer.
	 * Subclasses of timer_engine call this method when requested time interval expires.
	 */
	void timeout();
};

}
}
