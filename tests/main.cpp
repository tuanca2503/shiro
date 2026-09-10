#include <iostream>
#include "engine.h"

 
int main() {
std::unique_ptr<Engine> e = createLlamaEngine();
    e->loadModel("model.gguf", 99, 4096);
    std::string res = e->generate("Xin chào", 200);
    // KHÔNG cần gọi delete — unique_ptr tự động giải phóng khi ra khỏi scope
}

/* 
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TOOLS=OFF \
    -DLLAMA_BUILD_SERVER=OFF
*/
// cmake --build build -j 5
// ./build/daemon/daemon