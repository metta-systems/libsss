//! [Creating a host]
shared_ptr<host> my_host(make_shared<host>());
//! [Creating a host]

//! [Accepting a connection]
void some_accept_func(shared_ptr<sss::server> server)
{
    while (auto stream = server->accept()) {
        // Do something with a new stream.
    }
}
//! [Accepting a connection]
