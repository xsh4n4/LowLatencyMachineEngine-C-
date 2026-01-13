# Tests CMakeLists.txt

# Test client executable
add_executable(test_client test_client.cpp)

# Link with Boost libraries
target_link_libraries(test_client
    Boost::system
    Boost::thread
)

# Set C++ standard
set_target_properties(test_client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Add test client to tests target
add_custom_target(tests DEPENDS test_client)

# Install test client (optional)
install(TARGETS test_client DESTINATION bin)
