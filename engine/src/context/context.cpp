#include <algorithm>
#include <vector>

#include "context/context.h"
#include "template/gbnf-builder.h"

namespace shiro
{
    Context::~Context()
    {
        if (ctx_)
            llama_free(ctx_);
        llama_batch_free(batch_);
    }

    Context::Context(
        llama_model *model,
        int32_t n_threads,
        uint32_t n_ctx,
        uint32_t n_seq_max,
        bool kv_unified,
        PolicyType policy_type,
        int32_t max_tokens)
    {
        llama_context_params params = llama_context_default_params();
        params.n_ctx = n_ctx;
        params.n_seq_max = std::max(n_seq_max, 1u) + 1; // seq 0 = prompt
        params.n_threads = n_threads;
        params.kv_unified = kv_unified;
        //
        max_tokens_ = max_tokens;
        ctx_ = llama_init_from_model(model, params);
        vocab_ = llama_model_get_vocab(model);
        seq_policy_ = makeSequencePolicy(policy_type, ctx_, n_seq_max);
        sampler_policy_ = makeSamplerPolicy(policy_type, n_seq_max);
        batch_ = llama_batch_init(
            n_ctx, // number token max of one batch
            0,
            1 // mỗi token chỉ thuộc 1 sequence
        );
    }

    std::unique_ptr<Context> Context::createEphemeral(
        llama_model *model,
        int32_t n_threads,
        uint32_t n_ctx,
        uint32_t n_seq_max,
        bool kv_unified,
        int32_t max_tokens)
    {
        auto context = std::unique_ptr<Context>(
            new Context(
                model,
                n_threads,
                n_ctx,
                n_seq_max,
                kv_unified,
                PolicyType::Ephemeral,
                max_tokens));

        if (!context->ctx_)
            return nullptr;

        return context;
    }

    std::unique_ptr<Context> Context::createPersistent(
        llama_model *model,
        int32_t n_threads,
        uint32_t n_ctx,
        uint32_t n_seq_max,
        bool kv_unified,
        int32_t max_tokens)
    {
        auto context = std::unique_ptr<Context>(
            new Context(
                model,
                n_threads,
                n_ctx,
                n_seq_max,
                kv_unified,
                PolicyType::Persistent,
                max_tokens));

        if (!context->ctx_)
            return nullptr;

        return context;
    }

    //
    int32_t Context::countTokens(const std::string &text)
    {
        // llama_tokenize() with buffer NULL, length 0 will be return negative number,
        // The absolute value is the number of tokens required.
        return -llama_tokenize(vocab_, text.c_str(), text.size(), NULL, 0, true, true);
    }
    int32_t Context::decodeTokens(const llama_token *toks, int n, llama_seq_id seq_id, bool need_logits)
    {
        for (int i = 0; i < n; ++i)
        {
            batch_.token[i] = toks[i];
            batch_.pos[i] = seq_policy_->nPast(seq_id) + i;
            batch_.n_seq_id[i] = 1;
            batch_.seq_id[i][0] = seq_id;
            batch_.logits[i] = false;
        }
        batch_.logits[n - 1] = need_logits;
        batch_.n_tokens = n;

        int rc = llama_decode(ctx_, batch_);
        if (rc == 0)
            seq_policy_->advance(seq_id, n);
        return rc;
    }
    
    Result Context::setSystemPrompt(const std::string &full_prompt, llama_seq_id seq_id)
    {
        auto rsl = feedTokens(full_prompt, seq_id, false);
        if (rsl.success)
            seq_policy_->freeze(seq_id);
        return rsl;
    }
    Result Context::feedTokens(const std::string &text, llama_seq_id seq_id, bool need_logits)
    {
        const int32_t n_past = seq_policy_->nPast(seq_id);

        int n = countTokens(text);
        std::vector<llama_token> tokens(n);
        if (n < 0 || llama_tokenize(vocab_, text.c_str(), text.size(), tokens.data(), tokens.size(), n_past == 0, true) <= 0)
            return Result::failed("feedTokens.llama_tokenize>");

        if (int rc = decodeTokens(tokens.data(), n, seq_id, need_logits))
            return Result::failed("feedTokens.llama_decode> code: " + std::to_string(rc));
        return Result::ok();
    }
    Result Context::generate(llama_seq_id seq_id)
    {
        return generateStream([](const std::string &)
                              { return true; }, seq_id);
    }
    Result Context::generateStream(const ContextCallback &on_token, llama_seq_id seq_id)
    {
        llama_sampler *smpl = sampler_policy_->acquire(seq_id);
        if (!smpl)
            return Result::failed("generateStream> no sampler for seq");
        for (int i = 0; i < max_tokens_; i++)
        {
            llama_token new_id = llama_sampler_sample(smpl, ctx_, -1); // Predict the next word
            // llama_vocab_bos,llama_vocab_eos,llama_vocab_eot for feature return detail reason stop
            if (llama_vocab_is_eog(vocab_, new_id))
                break; // End generate

            char buf[128];
            int n = llama_token_to_piece(vocab_, new_id, buf, sizeof(buf), 0, true); // Token to text
            if (n < 0)
                return Result::failed("generateStream.llama_token_to_piece>");

            // Callback with new token. When callback return false,
            // that mean caller need stop (example user click stop) -> exit loop.
            if (!on_token(std::string(buf, n)))
            {
                llama_token toks[2] = {new_id, llama_vocab_eot(vocab_)};
                decodeTokens(toks, 2, seq_id, false);
                break;
            }

            if (int rc = decodeTokens(&new_id, 1, seq_id, true))
                return Result::failed("generateStream.llama_decode> cycle " + std::to_string(i) +
                                     " code " + std::to_string(rc));
        }
        return Result::ok();
    }

    // GRAMMAR
    Result Context::setToolCallGrammar(const std::string &json, const std::string &trigger, llama_seq_id seq_id)
    {
        std::string gbnf = buildGbnfToolCall(json, trigger);
        return setTurnGrammar(gbnf, seq_id, {trigger}, false);
    }
    Result Context::setRoutesGrammar(const std::vector<std::string> &routes, llama_seq_id seq_id)
    {
        std::string gbnf = buildGbnfPlainEnum(routes);
        return setTurnGrammar(gbnf, seq_id, {}, true); // eager, no need trigger
    }
    Result Context::setBooleanGrammar(llama_seq_id seq_id)
    {
        std::string gbnf = buildGbnfBoolean();
        return setTurnGrammar(gbnf, seq_id, {}, true);
    }
    Result Context::setJsonGrammar(const std::string &json, llama_seq_id seq_id)
    {
        std::string gbnf = buildGbnfJsonSchema(json);
        return setTurnGrammar(gbnf, seq_id, {}, true);
    }
    Result Context::setTurnGrammar(const std::string &gbnf, llama_seq_id seq_id, const std::vector<std::string> &triggers, bool eager)
    {
        llama_sampler_chain_params cp = llama_sampler_chain_default_params();
        llama_sampler *chain = llama_sampler_chain_init(cp);
        if (chain == nullptr)
            return Result::failed("setTurnGrammar.llama_sampler_chain_init> Can not create chain");

        if (!gbnf.empty())
        {
            llama_sampler *grmr = nullptr;
            // lazy mà không có trigger -> grammar sẽ không bao giờ bật.
            // Fallback sang eager để không mất ràng buộc.
            if (eager || triggers.empty())
                grmr = llama_sampler_init_grammar(vocab_, gbnf.c_str(), "root");
            else
            {
                std::vector<const char *> trigger_ptrs;
                trigger_ptrs.reserve(triggers.size());
                for (const auto &t : triggers)
                    trigger_ptrs.push_back(t.c_str());
                //
                grmr = llama_sampler_init_grammar_lazy_patterns(
                    vocab_,
                    gbnf.c_str(),
                    "root",
                    trigger_ptrs.data(), trigger_ptrs.size(),
                    nullptr, 0);
            }

            if (!grmr)
            {
                llama_sampler_free(chain);
                return Result::failed("setTurnGrammar.llama_sampler_init_grammar_lazy_patterns> Failed to build grammar sampler");
            }
            llama_sampler_chain_add(chain, grmr);
        }
        //
        llama_sampler_chain_add(chain, llama_sampler_init_top_k(40));
        llama_sampler_chain_add(chain, llama_sampler_init_temp(0.7f));
        llama_sampler_chain_add(chain, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
        // // Remove old sampler
        // if (sampler_ != nullptr)
        //     llama_sampler_free(sampler_);
        // sampler_ = chain;

        sampler_policy_->set(seq_id, chain);

        return Result::ok();
    }
}