cmake_minimum_required(VERSION 3.24)

project(HTTP_Servidor_Boost_Asio VERSION 1.0.0 LANGUAGES CXX)

option(HTTP_SERVER_BUILD_TESTS "Build automated tests" ON)

find_package(boost_asio CONFIG REQUIRED)
find_package(boost_system CONFIG REQUIRED)

add_library(http_server_core
    src/http_request.cpp
    src/http_response.cpp
    src/logger.cpp
    src/router.cpp
)
target_include_directories(http_server_core PUBLIC include)
target_link_libraries(http_server_core PUBLIC Boost::asio Boost::system)
target_compile_features(http_server_core PUBLIC cxx_std_20)

add_executable(http-server Main.cpp src/server.cpp src/session.cpp)
target_link_libraries(http-server PRIVATE http_server_core)

if(WIN32)
    target_compile_definitions(http_server_core PUBLIC _WIN32_WINNT=0x0A00)
endif()
if(MSVC)
    target_compile_options(http_server_core PRIVATE /W4 /permissive- /EHsc)
    target_compile_options(http-server PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(http_server_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(http-server PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET http-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/public"
            "$<TARGET_FILE_DIR:http-server>/public"
)

if(HTTP_SERVER_BUILD_TESTS)
    include(CTest)
    find_package(GTest CONFIG REQUIRED)
    add_executable(http-server-tests
        tests/test_http_request.cpp
        tests/test_http_response.cpp
        tests/test_router.cpp
    )
    target_link_libraries(http-server-tests PRIVATE http_server_core GTest::gtest_main)
    if(MSVC)
        target_compile_options(http-server-tests PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(http-server-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    include(GoogleTest)
    gtest_discover_tests(http-server-tests)
endif()
