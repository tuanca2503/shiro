
target_include_directories(agent
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(agent
    PRIVATE
        engine
)
