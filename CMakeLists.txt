cmake_minimum_required(VERSION 3.20)
project(MilleIpSwitch VERSION 1.0.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(milleipswitch WIN32 src/milleipswitch.cpp src/milleipswitch.rc)
target_compile_definitions(milleipswitch PRIVATE UNICODE _UNICODE)
target_link_libraries(milleipswitch PRIVATE
    iphlpapi ws2_32 shell32 advapi32 user32 comctl32 gdi32 ole32 oleaut32 shlwapi uuid taskschd setupapi cfgmgr32)

if(MSVC)
    target_compile_options(milleipswitch PRIVATE /utf-8 /W3)
    target_link_options(milleipswitch PRIVATE /MANIFEST:NO)
    set_property(TARGET milleipswitch PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
    target_compile_options(milleipswitch PRIVATE -municode -Wall -Wextra)
    target_link_options(milleipswitch PRIVATE -municode -static -s)
endif()
