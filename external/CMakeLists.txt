set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library")
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables")
set(WITH_TLS OFF CACHE BOOL "[ZMQ] Use TLS for WSS support")
set(WITH_LIBSODIUM OFF CACHE BOOL "[ZMQ] Use libsodium instead of built-in tweetnacl")
set(WITH_DOC OFF CACHE BOOL "[ZMQ] Build Reference Guide documentation(requires DocBook)")
set(WITH_DOCS OFF CACHE BOOL "[ZMQ] Build html docs")
set(BUILD_SHARED OFF CACHE BOOL "[ZMQ] Whether or not to build teh shared object")
set(BUILD_TESTS OFF CACHE BOOL "[ZMQ] Whether or not to build tests")
set(BUILD_TEST OFF CACHE BOOL "Build external test")

add_subdirectory(cppfs)
add_subdirectory(crypto)
add_subdirectory(cxxfolders)
add_subdirectory(ipv6-parse)
add_subdirectory(lmdb)
add_subdirectory(libzmq)
add_subdirectory(miniupnp/miniupnpc)
add_subdirectory(spdlog)

target_include_directories(ipv6-parser PUBLIC ipv6-parse/include)
target_include_directories(libminiupnpc-static PUBLIC miniupnp/miniupnpc)

set_property(TARGET
    cppfs
    crypto-static
    ipv6-parser
    platform_folders
    lmdb
    libzmq-static
    libminiupnpc-static
    spdlog
    PROPERTY FOLDER "external"
)

# this bundles up all external dependencies into a single target for linking elsewhere
add_library(External INTERFACE)
target_link_libraries(External INTERFACE
    cppfs
    crypto-static
    ipv6-parser
    platform_folders
    lmdb
    libzmq-static
    libminiupnpc-static
    spdlog
)
if(WIN32)
    target_link_libraries(External INTERFACE ws2_32 advapi32 crypt32 gdi32 user32)
endif()
target_include_directories(External INTERFACE
    cpplinenoise
    cxxopts/include
    cpphttplib
    cppzmq
)
