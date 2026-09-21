#pragma once
#include <memory>
#include <functional>
#include <llama.h>

#include "result.h"

#include "context/sampler-policy.h"
#include "context/sequence-policy.h"

namespace shiro
{
    using ContextCallback = std::function<bool(const std::string &token_text)>;
    // struct SequenceState
    // {
    //     int32_t n_past = 0;
    //     int32_t n_keep = 0;
    //     void freeze()
    //     {
    //         if (n_keep == 0)
    //             n_keep = n_past;
    //     }
    // };
    struct FeedRequest
    {
        llama_seq_id seq_id;
        const std::string &text;
        bool need_logits;
    };

    //
    class Context
    {
    public:
        ~Context();

        static std::unique_ptr<Context> createEphemeral(
            llama_model *model,
            int32_t n_threads,
            uint32_t n_ctx,
            uint32_t n_seq_max,
            bool kv_unified,
            int32_t max_tokens);

        static std::unique_ptr<Context> createPersistent(
            llama_model *model,
            int32_t n_threads,
            uint32_t n_ctx,
            uint32_t n_seq_max,
            bool kv_unified,
            int32_t max_tokens);

        void processFeedBatch(std::vector<FeedRequest> &&requests);

        int32_t countTokens(const std::string &text);
        int32_t decodeTokens(const llama_token *toks, int n, llama_seq_id seq_id, bool need_logits);
        Result setSystemPrompt(const std::string &prompt, llama_seq_id seq_id = 0);
        Result feedTokens(const std::string &text, llama_seq_id seq_id = 0, bool need_logits = false);
        Result generateStream(const ContextCallback &on_token, llama_seq_id seq_id = 0);
        Result generate(llama_seq_id seq_id);
        // GRAMMAR
        /// Configures grammar-constrained decoding for tool calls using the given
        /// JSON schema and trigger tag.
        Result setToolCallGrammar(const std::string &json, const std::string &trigger, llama_seq_id seq_id = 0);
        /// Configures grammar-constrained decoding to allow only the specified routes.
        Result setRoutesGrammar(const std::vector<std::string> &routes, llama_seq_id seq_id = 0);
        /// Configures grammar-constrained decoding for boolean output.
        Result setBooleanGrammar(llama_seq_id seq_id = 0);
        /// Configures grammar-constrained decoding using the given JSON schema.
        Result setJsonGrammar(const std::string &json, llama_seq_id seq_id = 0);
        /// Configures grammar-constrained decoding using a raw GBNF grammar.
        ///
        /// \param gbnf     GBNF grammar. An empty string disables grammar
        ///                 constraints.
        /// \param triggers Trigger strings that activate the grammar.
        /// \param eager    If true, apply the grammar immediately without waiting
        ///                 for a trigger.
        Result setTurnGrammar(const std::string &gbnf = "", llama_seq_id seq_id = 0, const std::vector<std::string> &triggers = {}, bool eager = false);

    private:
        Context(llama_model *model,
                int32_t n_threads,
                uint32_t n_ctx,
                uint32_t n_seq_max,
                bool kv_unified,
                PolicyType policy_type,
                int32_t max_tokens);

        llama_batch batch_{};
        llama_context *ctx_ = nullptr;
        const llama_vocab *vocab_ = nullptr;
        int32_t max_tokens_ = 0;
        std::unique_ptr<SamplerPolicy> sampler_policy_;
        std::unique_ptr<SequencePolicy> seq_policy_;
    };
}