add_executable(msh main.cpp shell_client.cpp)
target_link_libraries(msh shell sss routing krypto arsenal
    ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
install(TARGETS msh
    RUNTIME DESTINATION tools/shell)
