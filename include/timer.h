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
 * Implements interval timers suitable for network protocols.
 * Supports exponential backoff computations for retransmissions and retries,
 * as well as an optional "hard" failure deadline.
 * Also can be virtualized for simulation purposes.
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
    duration_type fail_interval_{fail_max};
    // boost::asio::deadline_timer::time_type deadline_; @todo For deadline timers.
    bool active_{false};
    bool failed_{false};

public:
    /**
     * Create a timer.
     * @param host Host state is used to create a new timer_engine for this timer.
     */
    timer(ssu::timer_host_state* host);

    /**
     * Start or restart the timer at a specified interval.
     * @todo Add support for default interval.
     * @todo Support deadline timers too.
     * @param interval the initial timer interval.
     * @param fail_interval the hard failure interval.
     */
   void start(duration_type interval = retry_min, duration_type fail_interval = fail_max);
    /**
     * Stop the timer if it is currently running.
     */
    void stop();
    /**
     * Restart the timer with a longer interval after a retry.
     */
    void restart();

    /**
     * Determine if we've reached the hard failure deadline.
     * @return true if the failure deadline has passed.
     */
    bool has_failed() const {
        return failed_;
    }

    /**
     * Determine if the timer is currently active.
     * @return true if the timer is ticking.
     */
    inline bool is_active() const {
        return active_;
    }
    /**
     * Obtain the timer's current interval.
     * @return the current timeout interval as set by call to start() or restart().
     */
    inline duration_type interval() const {
        return interval_;
    }

    /**
     * Accessed only by timer engine on timeouts. @fixme
     */
    void timeout_calculations();

    typedef boost::signals2::signal<void (bool)> timeout_signal;
    /**
     * Signaled when the timer expires.
     *
     * timeout signal handler signature is
     * void timeout(bool failed)
     * 
     * Argument 'failed' is true if the hard failure deadline has been reached.
     */
    timeout_signal on_timeout;
};

} // namespace async

/**
 * Base class providing hooks for time virtualization.
 * @todo The application may create a timer_host_state object and activate it using
 * time::set_timer_host_state(). The SSU protocol will then call the time factory's methods
 * whenever it needs to obtain the current system time or create timers.
 */
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
