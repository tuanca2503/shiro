#pragma once

#include <string>
#include <vector>
#include <memory>

#include "context.h"
#include "chat-types.h"

namespace shiro
{
    /// ENGINE: Context is session-scoped.
    ///
    /// - The engine keeps its KV cache across consecutive calls to
    ///   generate() / generateStream(); the context is not cleared
    ///   automatically between calls.
    ///
    /// - When the agent switches to a different conversation, it must
    ///   call clearContext() first, then send the complete token history
    ///   of that conversation back to the engine.
    ///
    /// - The agent is responsible for storing and retrieving conversation
    ///   history from its own database. The engine does not know about
    ///   conversations or persistent storage.
    class Engine
    {
    public:
        /// Destructor
        virtual ~Engine() = default;
        /// Load model from a .gguf file. n_gpu_layers: layers to offload to GPU (0 = CPU only).
        /// n_ctx: max context window size in tokens.
        virtual Result loadModel(const std::string &path, int n_gpu_layers) = 0;
        virtual Context *createContextEphemeral(uint32_t n_ctx = 4096, uint32_t n_seq_max = 1, bool kv_unified = true, int max_tokens = 1024) = 0;
        virtual Context *createContextPersistent(uint32_t n_ctx = 4096, uint32_t n_seq_max = 1, bool kv_unified = true, int max_tokens = 1024) = 0;
        virtual bool destroyContext(Context *ctx) = 0;
    };

    /// Factory function
    std::unique_ptr<Engine> createLlamaEngine();

}