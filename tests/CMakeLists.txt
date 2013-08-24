create_test(host "ssu;support;${OPENSSL_LIBRARIES}")
create_test(channel "ssu;support")
create_test(stream_user "ssu;support;${OPENSSL_LIBRARIES}")
create_test(stream_internal "ssu;support")
create_test(server "ssu;support;${OPENSSL_LIBRARIES}")

# These need a way to limit io_service run time (or a way to cancel async_receive_from).
#create_test(udp_link "ssu;support;${OPENSSL_LIBRARIES}")
#create_test(run_udp_receiver "ssu;support")

create_test(optional "ssu;support")
create_test(serialization "ssu;support")

# This needs to be sprinkled with BOOST_CHECK()s.
#create_test(crypto "ssu;support;${OPENSSL_LIBRARIES}")

create_test(sim_host "ssu;support;${OPENSSL_LIBRARIES}")
create_test(sim_link "ssu;support;${OPENSSL_LIBRARIES}")
create_test(sim_connection "ssu;support;${OPENSSL_LIBRARIES}")
