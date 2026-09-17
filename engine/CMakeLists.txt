# engine/ la 1 static library boc quanh llama.cpp.
# Chi export "include/" (chua engine.h) ra ngoai cho agent/daemon dung.
# Duong dan toi llama.cpp/include la PRIVATE -> khong lo ra ngoai engine.

add_library(engine STATIC
    include/chat-template.h
    include/chat-types.h
    include/engine.h
    include/gbnf-builder.h

    src/gbnf-builder.cpp
    src/llama-engine.cpp

    template/smollm3.cpp
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine
    PRIVATE
        llama
        llama-common 
        nlohmann_json::nlohmann_json
)
