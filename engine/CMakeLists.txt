# engine/ la 1 static library boc quanh llama.cpp.
# Chi export "include/" (chua engine.h) ra ngoai cho agent/daemon dung.
# Duong dan toi llama.cpp/include la PRIVATE -> khong lo ra ngoai engine.

add_library(engine STATIC
    include/chat-types.h
    include/context.h
    include/engine.h
    include/result.h
    include/policy-type.h
    include/chat-template.h
    
    
    src/context/sampler-policy.h
    src/context/sequence-policy.h

    src/context/context.cpp
    src/context/sampler-policy.cpp
    src/context/sequence-policy.cpp
    
    src/template/gbnf-builder.h
    src/template/gbnf-builder.cpp
    
    src/llama-engine.cpp
    src/template/chat-template.cpp
)

target_include_directories(engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(engine
    PUBLIC
        llama
        nlohmann_json::nlohmann_json

    PRIVATE
        llama-common
)
