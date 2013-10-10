//! [Accepting a connection]
void some_accept_func(shared_ptr<ssu::server> server)
{
    while (auto stream = server->accept()) {
        // Do something with a new stream.
    }
}
//! [Accepting a connection]
