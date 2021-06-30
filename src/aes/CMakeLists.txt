file(GLOB_RECURSE AES *)

source_group("" FILES ${AES})

add_library(AES STATIC ${AES})

target_link_libraries(AES External Errors)

target_include_directories(AES PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
