#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "chat_types.h"

using StreamCallback = std::function<bool(const std::string &token_text)>;
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
// ENGINE: Only using context session in time
// - Engine tu giu KV-cache xuyen suot cac lan goi generate()/generateStream()
//   lien tiep (khong tu xoa giua cac lan).
// - Khi agent muon chuyen sang 1 cuoc hoi thoai KHAC, agent tu goi
//   clearContext() truoc, roi gui lai TOAN BO token cua hoi thoai do
//   (agent tu luu/truy van tu DB rieng, Engine khong biet gi ve "hoi thoai"
//   hay "luu tru" - do la trach nhiem cua agent).
class Engine
{
public:
    // Destructor
    virtual ~Engine() = default;
    virtual void clearContext() = 0;
    virtual int getContextSize() = 0;
    virtual int getContextUsage() = 0;
    virtual int countTokens(const std::string &text) = 0;

    // Load model from a .gguf file. n_gpu_layers: layers to offload to GPU (0 = CPU only).
    // n_ctx: max context window size in tokens.
    virtual EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) = 0;
    // Remove the oldest n_tokens_to_remove tokens from the KV-cache and shift
    // remaining positions back. Never removes into the system prompt region.
    virtual EngineResult trimContext(int n_tokens_to_remove) = 0;
    // Format a message list (system/user/assistant/tool) using the model's
    // chat template. Returns the formatted prompt as text.
    virtual EngineResult applyChatTemplate(const std::vector<ChatMessage> &messages) = 0;
    // Decode a system prompt into the cache and mark its end position as
    // protected (trimContext() will never remove past this point).
    virtual EngineResult setSystemPrompt(const std::string &system_prompt) = 0;
    // Generate a reply for the given prompt, up to max_tokens new tokens.
    // Waits for the full result before returning.
    virtual EngineResult generate(const std::string &prompt, int max_tokens) = 0;
    // Same as generate(), but calls on_token for every token as it's produced.
    virtual EngineResult generateStream(const std::string &full_prompt, int max_tokens, const StreamCallback &on_token) = 0;
};

// Factory function
std::unique_ptr<Engine> createLlamaEngine();
