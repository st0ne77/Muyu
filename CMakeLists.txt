cmake_minimum_required(VERSION 3.14.1)
cmake_policy(SET CMP0091 NEW)
project(Muyu VERSION 1.0.1.4 LANGUAGES C CXX)

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    add_compile_options(/wd4251 /wd4275)
    add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_CRT_NON_CONFORMING_SWPRINTFS -DNOMINMAX)
endif(MSVC)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc)

add_executable(Muyu WIN32
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
    main.cpp
    window.cpp
)

target_include_directories(Muyu
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Muyu
    gdiplus
    winmm
)

set_target_properties(Muyu PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    LINK_FLAGS "/SAFESEH:NO"
)