# Linux is notorious for it's awkward library handling.
# There should be some more work on separating the librouting and libssu so that
# there are no cross-references and libraries could be linked completely independently.
if (UNIX AND NOT APPLE)
    set(SSULIBS routing ssu routing ssu)
else()
    set(SSULIBS routing ssu)
endif()

create_test(identity LIBS ssu support ${OPENSSL_LIBRARIES})

create_test(host LIBS ssu support krypto ${OPENSSL_LIBRARIES})
create_test(channel LIBS ssu support)
create_test(stream_user LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})
create_test(stream_internal LIBS ssu support)
create_test(server LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})

create_test(optional LIBS ssu support)
create_test(serialization LIBS ssu support)

create_test(sim_host LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})
create_test(sim_link LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})
create_test(sim_connection LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})
create_test(simulator LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES})

# Regression tests are fairly long
create_test(datagrams LIBS ${SSULIBS} support krypto ${OPENSSL_LIBRARIES} NO_CTEST)

# These need a way to limit io_service run time (or a way to cancel async_receive_from).
create_test(udp_link LIBS ssu support krypto ${OPENSSL_LIBRARIES} NO_CTEST)
create_test(run_udp_receiver LIBS ssu support krypto ${OPENSSL_LIBRARIES} NO_CTEST)
