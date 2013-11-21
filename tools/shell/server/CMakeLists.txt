add_executable(mshd main.cpp shell_server.cpp shell_session.cpp pid_watcher.cpp)
target_link_libraries(mshd shell ssu routing krypto support plist ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
install(TARGETS mshd
    RUNTIME DESTINATION tools/shell)

#add_definitions(-DETCDIR="/usr/local/etc")
