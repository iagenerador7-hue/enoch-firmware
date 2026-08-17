add_library(usermod_enoch_i2s INTERFACE)

target_sources(usermod_enoch_i2s INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/enoch_i2s.c
)

target_include_directories(usermod_enoch_i2s INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_enoch_i2s)
