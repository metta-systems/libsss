//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/positional_options.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "ssu/host.h"
#include "shell_client.h"
#include "arsenal/settings_provider.h"

using namespace ssu;
using namespace std;
namespace po = boost::program_options;

int main(int argc, char **argv)
{
    string nickname;
    vector<string> location_hints;
    int port{stream_protocol::default_port};
    bool verbose_debug{false};
    string peer;

    po::options_description desc("msh mettashell client");
    desc.add_options()
        ("nickname,n", po::value<string>(&nickname),
            "Custom shorthand name for the remote host")
        ("eid,e", po::value<string>(&peer),
            "EID of the remote host")
        ("host,h", po::value<vector<string>>(&location_hints),
            "Endpoint location (ipv4 or ipv6 address), can be specified multiple times")
        ("port,p", po::value<int>(&port)->default_value(stream_protocol::default_port),
            "Connect to service on this port")
        ("verbose,v", po::bool_switch(&verbose_debug),
            "Print verbose output for debug")
        ("help",
            "Print this help message");
    po::positional_options_description p;
    p.add("nickname", 1);
    p.add("eid", 1);
    p.add("host", 1);
    p.add("port", 1);
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
          options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << "Usage: msh <nickname> [<eid> [<hostname> [<port>]]]\n\n";
        cout << desc << endl;
        return 1;
    }

    if (!verbose_debug) {
        logger::set_verbosity(logger::verbosity::info);
    }

    // @todo Resolve host names...

    // Convert the list of addresses to a list of endpoints,
    // using the same port number for each.
    // vector<endpoint> eps;
    // for (const QHostAddress &addr, addrs)
        // eps.append(Endpoint(addr, port));

    auto settings = settings_provider::instance();

    // Initialize SST and read or create our own host identity
    shared_ptr<host> host(host::create(settings.get()));

    peer_id eid{peer};

    // Find any existing information we have about the requested nickname.
    // privsettings->beginGroup(QString("nickname:") + nickname);
    // byte_array eid = byte_array::fromBase64(privsettings->value("eid").toString().toAscii());
    // XX address hints?
    // privsettings->endGroup();

    if (eid.is_empty() and location_hints.empty())
    {
        logger::fatal() << "Host nickname '" << nickname
            << "' not known: please specify host's DNS name or IP address.";
    }

    logger::info() << "Connecting to " << eid;

    // Connect to the shell service
    shell_client sc(host);
    ssu::endpoint init_ep(boost::asio::ip::address::from_string(location_hints[0]), port);
    sc.connect_to(eid, init_ep);

    // Register the list of target address hints
    for (auto epstr : location_hints)
    {
        ssu::endpoint ep(boost::asio::ip::address::from_string(epstr), port);
        logger::debug() << "Connecting at location hint " << ep;
        sc.connect_at(ep);
    }

    // Set up the pseudo-tty on the server side, if appropriate.
    if (isatty(STDIN_FILENO))
        sc.setup_terminal(STDIN_FILENO);

    // Set up data forwarding
    string cmd;    // XXX just a plain shell for now
    sc.run_shell(cmd, STDIN_FILENO, STDOUT_FILENO);

    host->run_io_service();
}
