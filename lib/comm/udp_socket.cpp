#include "ssu/host.h" // @todo Remove, temporarily used to make socket.h below compile
// when decoupled, should not need host.h include above

#include "ssu/udp_socket.h"
#include "comm/platform.h"

using namespace std;
using namespace boost::asio;

namespace ssu {

//=================================================================================================
// helper function
//=================================================================================================

bool bind_socket(boost::asio::ip::udp::socket& sock,
                 uia::comm::endpoint const& ep,
                 std::string& error_string)
{
    boost::system::error_code ec;
    sock.open(ep.protocol(), ec);
    if (ec) {
        error_string = ec.message();
        logger::warning() << "udp socket open error - " << ec.message();
        return false;
    }
    sock.bind(ep, ec);
    if (ec) {
        error_string = ec.message();
        logger::warning() << "udp socket bind error - " << ec.message();
        return false;
    }
    error_string = "";
    return true;
}

//=================================================================================================
// udp_socket
//=================================================================================================

udp_socket::udp_socket(shared_ptr<host> host)
    : socket(host)
    , udp_socket_(host->get_io_service())
    , received_from_(this, uia::comm::endpoint()) // @fixme Dummy endpoint initializer here... init in bind()?
    , strand_(host->get_io_service())
{}

/**
 * See http://stackoverflow.com/questions/12794107/why-do-i-need-strand-per-connection
 * Run prepare_async_receive() through a strand always to make this operation thread safe.
 */
void
udp_socket::prepare_async_receive()
{
    boost::asio::streambuf::mutable_buffers_type buffer = received_buffer_.prepare(2048);
    udp_socket_.async_receive_from(
        boost::asio::buffer(buffer),
        received_from_,
        boost::bind(&udp_socket::udp_ready_read, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
}

vector<uia::comm::endpoint>
udp_socket::local_endpoints()
{
    vector<uia::comm::endpoint> result{udp_socket_.local_endpoint()};
    auto addresses = uia::comm::platform::local_endpoints();
    auto port = local_port();
    for (auto v : addresses) {
        v.port(port);
        result.emplace_back(v);
    }
    return result;
}

uint16_t
udp_socket::local_port()
{
    return udp_socket_.local_endpoint().port();
}

bool
udp_socket::bind(uia::comm::endpoint const& ep)
{
    // if (ep.address().is_v6()) {
        // udp_socket.set_option(ip::v6_only(true));
    // }
    logger::debug() << "udp_socket bind on endpoint " << ep;
    if (!bind_socket(udp_socket_, ep, error_string_))
        return false;
    logger::debug() << "Bound udp_socket on " << ep;
    // once bound, can start receiving datagrams.
    prepare_async_receive();
    set_active(true);
    return true;
}

void
udp_socket::unbind()
{
    logger::debug() << "udp_socket unbind";
    udp_socket_.shutdown(ip::udp::socket::shutdown_both);
    udp_socket_.close();
    set_active(false);
}

bool
udp_socket::send(uia::comm::endpoint const& ep, char const* data, size_t size)
{
    boost::system::error_code ec;
    size_t sent = udp_socket_.send_to(buffer(data, size), ep, 0, ec);
    if (ec or sent < size) {
        error_string_ = ec.message();
    }
    return sent == size;
}

void
udp_socket::udp_ready_read(boost::system::error_code const& error, size_t bytes_transferred)
{
    if (!error)
    {
        logger::debug() << "Received "
            << dec << bytes_transferred << " bytes via UDP link from " << received_from_
            << " on link " << this;
        byte_array b(buffer_cast<char const*>(received_buffer_.data()), bytes_transferred);
        receive(b, received_from_);
        received_buffer_.consume(bytes_transferred);
        strand_.dispatch([this] { prepare_async_receive(); });
    }
    else
    {
        error_string_ = error.message();
        logger::warning() << "UDP read error - " << error_string_;
    }
}

} // ssu namespace
