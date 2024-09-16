# TCP echo server.
add_executable(tcp-echo "tcp-echo/main.cpp")
target_link_libraries(tcp-echo PRIVATE nyaio::nyaio)

# UDP echo server.
add_executable(udp-echo "udp-echo/main.cpp")
target_link_libraries(udp-echo PRIVATE nyaio::nyaio)
