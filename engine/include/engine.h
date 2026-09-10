#pragma once

#include <string>
#include <memory>

struct EngineResult
{
    bool success = false;
    std::string text = "";

    static EngineResult ok(std::string text = "")
    {
        return EngineResult{true, std::move(text)};
    }

    static EngineResult failed(std::string error_msg)
    {
        return EngineResult{false, std::move(error_msg)};
    }
};

class Engine
{
public:
    virtual ~Engine() = default; // Destructor
    virtual EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) = 0;
    virtual EngineResult generate(const std::string &prompt, int max_tokens) = 0;
};

// Factory function
std::unique_ptr<Engine> createLlamaEngine();
