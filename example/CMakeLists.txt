# TCP echo server.
add_executable(echo "echo/main.cpp")
target_link_libraries(echo PRIVATE nyaio::nyaio)
