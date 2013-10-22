create_test(identity LIBS ssu support plist ${OPENSSL_LIBRARIES})

create_test(host LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})
create_test(channel LIBS ssu support plist)
create_test(stream_user LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})
create_test(stream_internal LIBS ssu support plist)
create_test(server LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})

create_test(optional LIBS ssu support plist)
create_test(serialization LIBS ssu support plist)

create_test(sim_host LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})
create_test(sim_link LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})
create_test(sim_connection LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})
create_test(simulator LIBS ssu support plist krypto ${OPENSSL_LIBRARIES})

# These need a way to limit io_service run time (or a way to cancel async_receive_from).
create_test(udp_link LIBS ssu support plist krypto ${OPENSSL_LIBRARIES} NO_CTEST)
create_test(run_udp_receiver LIBS ssu support plist krypto ${OPENSSL_LIBRARIES} NO_CTEST)
