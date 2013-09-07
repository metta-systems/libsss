create_test(host LIBS ssu support ${OPENSSL_LIBRARIES})
create_test(channel LIBS ssu support)
create_test(stream_user LIBS ssu support ${OPENSSL_LIBRARIES})
create_test(stream_internal LIBS ssu support)
create_test(server LIBS ssu support ${OPENSSL_LIBRARIES})

create_test(optional LIBS ssu support)
create_test(serialization LIBS ssu support)

create_test(sim_host LIBS ssu support ${OPENSSL_LIBRARIES})
create_test(sim_link LIBS ssu support ${OPENSSL_LIBRARIES})
create_test(sim_connection LIBS ssu support ${OPENSSL_LIBRARIES})
create_test(simulator LIBS ssu support ${OPENSSL_LIBRARIES})

# This needs to be sprinkled with BOOST_CHECK()s.
create_test(crypto LIBS ssu support ${OPENSSL_LIBRARIES})

# These need a way to limit io_service run time (or a way to cancel async_receive_from).
create_test(udp_link LIBS ssu support ${OPENSSL_LIBRARIES} NO_CTEST)
create_test(run_udp_receiver LIBS ssu support NO_CTEST)
