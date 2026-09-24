#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <llama.h>

#include "result.h"
#include "policy-type.h"

namespace shiro
{

    class SequencePolicy;
    class SamplerPolicy;

    using ContextCallback = std::function<bool(const std::string &token_text)>;
    struct FeedRequest
    {
        int32_t seq_id;
        const std::string &text;
        bool need_logits;
    };

    //
    class Context
    {
    public:
        ~Context();

        Context(llama_model *model,
                int32_t n_threads,
                uint32_t n_ctx,
                uint32_t n_seq_max,
                bool kv_unified,
                PolicyType policy_type,
                int32_t max_tokens);

        bool isValid() const noexcept { return ctx_ != nullptr; }

        void processFeedBatch(std::vector<FeedRequest> &&requests);

        uint32_t nCtx();
        uint32_t nSeqMax();
        int32_t countTokens(const std::string &text);
        int32_t decodeTokens(const llama_token *toks, int n, bool need_logits, int32_t seq_id);

        void acquire(const std::string &system_prompt = "", int32_t seq_id = 0);
        void release(int32_t seq_id = 0);
        Result feedTokens(const std::string &text, bool need_logits = false, int32_t seq_id = 0);
        Result generateStream(const ContextCallback &on_token, int32_t seq_id = 0);
        Result generate(int32_t seq_id);
        // GRAMMAR
        /// Configures grammar-constrained decoding for tool calls using the given
        /// JSON schema and trigger tag.
        Result setToolCallGrammar(const std::string &json, const std::string &trigger, int32_t seq_id = 0);
        /// Configures grammar-constrained decoding to allow only the specified routes.
        Result setRoutesGrammar(const std::vector<std::string> &routes, int32_t seq_id = 0);
        /// Configures grammar-constrained decoding for boolean output.
        Result setBooleanGrammar(int32_t seq_id = 0);
        /// Configures grammar-constrained decoding using the given JSON schema.
        Result setJsonGrammar(const std::string &json, int32_t seq_id = 0);
        /// Configures grammar-constrained decoding using a raw GBNF grammar.
        ///
        /// \param gbnf     GBNF grammar. An empty string disables grammar
        ///                 constraints.
        /// \param triggers Trigger strings that activate the grammar.
        /// \param eager    If true, apply the grammar immediately without waiting
        ///                 for a trigger.
        Result setTurnGrammar(const std::string &gbnf = "", int32_t seq_id = 0, const std::vector<std::string> &triggers = {}, bool eager = false);

    private:
        llama_batch batch_{};
        llama_context *ctx_ = nullptr;
        const llama_vocab *vocab_ = nullptr;
        int32_t max_tokens_ = 0;
        std::unique_ptr<SamplerPolicy> sampler_policy_;
        std::unique_ptr<SequencePolicy> seq_policy_;
    };
}