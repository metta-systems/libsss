// SimLink
namespace ssu {
namespace simulation {

enum LinkPreset {
    DSL15,      ///< 1.5Mbps/384Kbps DSL link
    Cable5,     ///< 5Mbps cable modem link
    Sat10,      ///< 10Mbps satellite link with 500ms delay
    Eth10,      ///< 10Mbps Ethernet link
    Eth100,     ///< 100Mbps Ethernet link
    Eth1000,    ///< 1000Mbps Ethernet link
};

} // simulation namespace
} // ssu namespace
