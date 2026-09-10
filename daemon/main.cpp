// daemon/main.cpp
// File nay KHONG include llama.h, khong biet class LlamaEngine ton tai.
// Chi biet duy nhat "engine.h" va ham factory createLlamaEngine().

#include <iostream>
#include <memory>
#include "engine.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Cach dung: " << argv[0] << " <duong-dan-model.gguf>\n";
        return 1;
    }

    std::string model_path = argv[1];

    // Tao engine qua factory function - khong biet implementation ben trong
    std::unique_ptr<Engine> engine(createLlamaEngine());

    std::cout << "Dang load model: " << model_path << " ...\n";

    // n_gpu_layers = 0 -> chay thuan CPU (an toan mac dinh cho test)
    // n_ctx = 2048 -> du de test nhanh
    EngineResult rsl = engine->loadModel(model_path, /*n_gpu_layers=*/0, /*n_ctx=*/2048);
    if (!rsl.success)
    {
        std::cerr << rsl.text;
        return 1;
    }

    std::cout << "Load model thanh cong. Go cau hoi (go 'exit' de thoat):\n";

    std::string line;
    while (true)
    {
        std::cout << "\nBan: ";
        if (!std::getline(std::cin, line))
            break;
        if (line == "exit")
            break;
        if (line.empty())
            continue;
        rsl = engine->generate(line, /*max_tokens=*/200);
        std::cout << (rsl.success ? "Model: " : "ERROR SYSTEM: ") << rsl.text << "\n";
    }

    return 0;
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