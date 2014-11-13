//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "comm/udp_socket.h"
#include "sss/host.h"

using namespace std;
using namespace sss;
using namespace uia;

int main()
{
	try
	{
		shared_ptr<host> host(make_shared<host>());
		comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
		udp_socket l(host);
		l.bind(local_ep);
		l.send(local_ep, "\0SSSohai!", 10);
		host->run_io_service();
	}
	catch (exception& e)
	{
		cerr << e.what() << endl;
	}
}
