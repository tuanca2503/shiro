#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "types/chat-types.h"
namespace shiro
{
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
        /// Clears the current session context and KV cache.
        virtual void clearContext() = 0;
        /// Returns the total context window size in tokens.
        virtual int getContextSize() = 0;
        /// Returns the number of tokens currently used in the context.
        virtual int getContextUsage() = 0;
        /// Returns the number of tokens produced by tokenizing the given text.
        virtual int countTokens(const std::string &text) = 0;
        //
        /// Load model from a .gguf file. n_gpu_layers: layers to offload to GPU (0 = CPU only).
        /// n_ctx: max context window size in tokens.
        virtual EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) = 0;
        /// Remove the oldest n_tokens_to_remove tokens from the KV-cache and shift
        /// remaining positions back. Never removes into the system prompt region.
        virtual EngineResult trimContext(int n_tokens_to_remove) = 0;
        /// Format a message list (system/user/assistant/tool) using the model's
        /// chat template. Returns the formatted prompt as text.
        virtual EngineResult applyChatTemplate(const std::vector<ChatMessage> &messages, bool add_generation_prompt = true, bool enable_thinking = true) = 0;
        /// Decode a system prompt into the cache and mark its end position as
        /// protected (trimContext() will never remove past this point).
        virtual EngineResult applySystemTemplate(const std::string &system_prompt = "", const std::vector<std::string> &tools = {}, bool enable_thinking = true) = 0;
        /// Tokenizes and decodes raw text into the KV cache without sampling.
        /// Used to feed context (e.g. system prompt) the model should know but not respond to.
        virtual EngineResult feedTokens(const std::string &text) = 0;
        /// Generate a reply for the given prompt, up to max_tokens new tokens.
        /// Waits for the full result before returning.
        virtual EngineResult generate(int max_tokens) = 0;
        /// Same as generate(), but calls on_token for every token as it's produced.
        virtual EngineResult generateStream(int max_tokens, const StreamCallback &on_token) = 0;
        // GRAMMAR
        /// Configures grammar-constrained decoding for tool calls using the given
        /// JSON schema and trigger tag.
        virtual EngineResult setToolCallGrammar(const std::string &json, const std::string &trigger) = 0;
        /// Configures grammar-constrained decoding to allow only the specified routes.
        virtual EngineResult setRoutesGrammar(const std::vector<std::string> &routes) = 0;
        /// Configures grammar-constrained decoding for boolean output.
        virtual EngineResult setBooleanGrammar() = 0;
        /// Configures grammar-constrained decoding using the given JSON schema.
        virtual EngineResult setJsonGrammar(const std::string &json) = 0;
        /// Configures grammar-constrained decoding using a raw GBNF grammar.
        ///
        /// \param gbnf     GBNF grammar. An empty string disables grammar
        ///                 constraints.
        /// \param triggers Trigger strings that activate the grammar.
        /// \param eager    If true, apply the grammar immediately without waiting
        ///                 for a trigger.
        virtual EngineResult setTurnGrammar(const std::string &gbnf = "", const std::vector<std::string> &triggers = {}, bool eager = false) = 0;
    };

    /// Factory function
    std::unique_ptr<Engine> createLlamaEngine();

}