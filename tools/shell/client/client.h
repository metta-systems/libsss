#include "shell_protocol.h"
#include "shell_stream.h"

class shell_client : public shell_protocol
{
private:
    ssu::stream stream_;
    shell_stream shs;
    async_file afin, afout;

public:
    shell_client(SST::Host *host, QObject *parent = NULL);

    inline void connect_to(const SST::PeerId &dsteid, const SST::Endpoint &locationHint)
    {
        Q_ASSERT(!strm.isConnected());
        stream_.connect_to(dsteid, serviceName, protocolName, locationHint);
    }

    inline void connect_at(ssu::endpoint const& ep)
    {
        stream_.connect_at(ep);
    }

    void setup_terminal(int fd);
    void run_shell(std::string const& cmd, int infd, int outfd);

private:
    void got_control_packet(byte_array const& msg);

    // Handlers
    void in_ready();
    void out_ready();
};
