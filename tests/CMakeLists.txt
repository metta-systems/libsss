# Linux is notorious for it's awkward library handling.
# There should be some more work on separating the librouting and libssu so that
# there are no cross-references and libraries could be linked completely independently.
if (UNIX AND NOT APPLE)
    set(SSULIBS routing ssu routing ssu)
else()
    set(SSULIBS routing ssu)
endif()

create_test(identity LIBS ssu arsenal ${OPENSSL_LIBRARIES})

create_test(host LIBS ssu arsenal krypto ${OPENSSL_LIBRARIES})
create_test(channel LIBS ssu arsenal)
create_test(stream_user LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})
create_test(stream_internal LIBS ssu arsenal)
create_test(server LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})

create_test(optional LIBS ssu arsenal)
create_test(serialization LIBS ssu arsenal)

create_test(sim_host LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})
create_test(sim_link LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})
create_test(sim_connection LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})
create_test(simulator LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES})

# Regression tests are fairly long
create_test(datagrams LIBS ${SSULIBS} arsenal krypto ${OPENSSL_LIBRARIES} NO_CTEST)

# These need a way to limit io_service run time (or a way to cancel async_receive_from).
create_test(udp_link LIBS ssu arsenal krypto ${OPENSSL_LIBRARIES} NO_CTEST)
create_test(run_udp_receiver LIBS ssu arsenal krypto ${OPENSSL_LIBRARIES} NO_CTEST)
