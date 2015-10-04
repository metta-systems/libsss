# Linux is notorious for it's awkward library handling.
# There should be some more work on separating the librouting and libsss so that
# there are no cross-references and libraries could be linked completely independently.
if (UNIX AND NOT APPLE)
    set(SSS_LIBS routing sss routing sss uia)
else()
    set(SSS_LIBS routing sss uia)
endif()

create_test(frames_serialization LIBS sss arsenal)

create_test(host LIBS ${SSS_LIBS} arsenal routing ${OPENSSL_LIBRARIES})
create_test(channel LIBS sss arsenal)
create_test(stream_user LIBS ${SSS_LIBS} arsenal sodiumpp ${OPENSSL_LIBRARIES})
create_test(stream_internal LIBS sss arsenal)
create_test(substreams LIBS ${SSS_LIBS} arsenal sodiumpp ${OPENSSL_LIBRARIES})
create_test(server LIBS ${SSS_LIBS} arsenal sodiumpp ${OPENSSL_LIBRARIES})
create_test(negotiation LIBS ${SSS_LIBS} arsenal routing sodiumpp)

create_test(optional LIBS sss arsenal)
create_test(serialization LIBS sss arsenal)

create_test(sim_host LIBS ${SSS_LIBS} arsenal ${OPENSSL_LIBRARIES})
create_test(sim_link LIBS ${SSS_LIBS} arsenal ${OPENSSL_LIBRARIES})
create_test(sim_connection LIBS ${SSS_LIBS} arsenal ${OPENSSL_LIBRARIES})
create_test(simulator LIBS ${SSS_LIBS} arsenal sodiumpp ${OPENSSL_LIBRARIES})

# Regression tests are fairly long
create_test(datagrams LIBS ${SSS_LIBS} arsenal sodiumpp ${OPENSSL_LIBRARIES} NO_CTEST)
