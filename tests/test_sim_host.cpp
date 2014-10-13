//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_sim_host
#include <boost/test/unit_test.hpp>

#include "sss/simulation/simulator.h"
#include "sss/simulation/sim_host.h"

using namespace std;
using namespace sss;
using namespace sss::simulation;

BOOST_AUTO_TEST_CASE(created_host)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    shared_ptr<sim_host> my_host(sim_host::create(sim));
}
