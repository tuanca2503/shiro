# engine/ la 1 static library boc quanh llama.cpp.
# Chi export "include/" (chua engine.h) ra ngoai cho agent/daemon dung.
# Duong dan toi llama.cpp/include la PRIVATE -> khong lo ra ngoai engine.

add_library(engine STATIC
    include/context/context-manager.h
    include/context/context.h

    include/types/chat-types.h
    include/engine.h
    include/result.h
    
    src/context/policy-type.h
    src/context/sampler-policy.h
    src/context/sequence-policy.h
    src/context/context-manager.cpp
    src/context/context.cpp
    
    src/template/chat-template.h
    src/template/gbnf-builder.cpp
    src/template/gbnf-builder.h
    src/template/smollm3-template.cpp
    
    src/llama-engine.cpp
    src/context/sampler-policy.cpp
    src/context/sequence-policy.cpp
)

target_include_directories(engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(engine
    PRIVATE
        llama
        llama-common 
        nlohmann_json::nlohmann_json
)
