//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <stdlib.h>
#include <unistd.h>
#include "sss/internal/platform.h"

using namespace std;

namespace sss {
namespace platform {

std::string user_name()
{
    char const* username = getenv("USER");
    if (username == nullptr)
        username = getenv("LOGNAME");
    if (username == nullptr)
        username = getlogin();
    if (username == nullptr)
        username = "Someone";
    return std::string(username);
}

} // platform namespace
} // sss namespace
