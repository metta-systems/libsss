//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/asio.hpp>
#include "sss/internal/timer.h"
#include "sss/internal/timer_engine.h"
#include "arsenal/make_unique.h"

namespace bp = boost::posix_time;

namespace sss {
namespace async {

/**
 * Default initial retry time: 500 ms
 */
const timer::duration_type timer::retry_min = bp::milliseconds(500);
/**
 * Default max retry period: 1 minute
 */
const timer::duration_type timer::retry_max = bp::minutes(1);
/**
 * Default hard failure deadline: 20 seconds
 */
const timer::duration_type timer::fail_max  = bp::seconds(20);

//=================================================================================================
// timer
//=================================================================================================

/**
 * Exponential backoff function for retry.
 */
static timer::duration_type
backoff(timer::duration_type interval, timer::duration_type max_interval = timer::fail_max)
{
    return std::min(interval * 3 / 2, max_interval);
}

timer::timer(timer_host_state* host)
{
    engine_ = host->create_timer_engine_for(this);
}

void timer::start(duration_type interval, duration_type fail_interval)
{
    interval_ = interval;
    fail_interval_ = fail_interval;
    active_ = true;
    engine_->start(interval_);
}

void timer::stop()
{
    engine_->stop();
    active_ = false;
}

void timer::restart()
{
    interval_ = backoff(interval_);
    start(interval_);
}

void timer::timeout_calculations()
{
    fail_interval_ -= interval_;
    failed_ = (fail_interval_ <= bp::milliseconds(0));
}

//=================================================================================================
// default_timer_engine
//=================================================================================================

/**
 * Default boost.asio implementation of the timer_engine.
 */
class default_timer_engine : public timer_engine
{
    boost::asio::deadline_timer interval_timer;

    void asio_timeout(boost::system::error_code const& error);

public:
    default_timer_engine(timer* t, boost::asio::io_service& io_service);
    ~default_timer_engine();

    void start(duration_type interval) override;
    void stop() override;
};

default_timer_engine::default_timer_engine(timer* t, boost::asio::io_service& io_service)
    : timer_engine(t)
    , interval_timer(io_service)
{
}

default_timer_engine::~default_timer_engine()
{
    stop();
}

void default_timer_engine::start(duration_type interval)
{
    interval_timer.expires_from_now(interval);
    interval_timer.async_wait(
        boost::bind(&default_timer_engine::asio_timeout, this, boost::asio::placeholders::error));
}

void default_timer_engine::stop()
{
    interval_timer.cancel();
}

/**
 * Boost.asio deadline_timer will call this function even when timer is cancelled.
 * Check the error code and do NOT call on_timeout() if this invocation is due to
 * timer cancellation.
 */
void default_timer_engine::asio_timeout(boost::system::error_code const& error)
{
    if (error == boost::asio::error::operation_aborted)
        return;

    timeout();
}

//=================================================================================================
// timer_engine
//=================================================================================================

void timer_engine::timeout()
{
    origin_->timeout_calculations();
    origin_->on_timeout(origin_->has_failed());
}

} // namespace async

//=================================================================================================
// timer_host_state
//=================================================================================================

boost::posix_time::ptime timer_host_state::current_time()
{
    return boost::posix_time::microsec_clock::local_time();
}

std::unique_ptr<async::timer_engine> timer_host_state::create_timer_engine_for(async::timer* t)
{
    return stdext::make_unique<async::default_timer_engine>(t, io_service_);
}

} // sss namespace
