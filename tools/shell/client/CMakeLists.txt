add_executable(msh main.cpp cli.cpp ../proto.cpp ../asyncfile.cpp)
target_link_libraries(nsh sst routing ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
install(TARGETS msh
    RUNTIME DESTINATION tools/shell)
