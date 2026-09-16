#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <llama.h>

#include "engine.h"
#include "chat-template.h"
#include "gbnf-builder.h"

namespace
{
    class LlamaEngine : public Engine
    {
    public:
        LlamaEngine()
        {
            llama_log_set([](enum ggml_log_level level, const char *text, void *user_data)
                          {
        (void)level;
        (void)text;
        (void)user_data; }, nullptr);
            llama_backend_init();
            llama_numa_init(GGML_NUMA_STRATEGY_DISABLED); // GGML_NUMA_STRATEGY_DISTRIBUTE de tan dung dung NUMA
        }
        ~LlamaEngine() override
        {
            if (sampler_)
                llama_sampler_free(sampler_);
            if (ctx_)
                llama_free(ctx_);
            if (model_)
                llama_model_free(model_);
            llama_backend_free();
        }
        void clearContext() override
        {
            if (!ctx_)
                return;
            llama_memory_t mem = llama_get_memory(ctx_);
            llama_memory_clear(mem, true);
            n_past_ = 0;
        }
        int getContextSize() override
        {
            return ctx_ ? (int)llama_n_ctx(ctx_) : 0;
        }
        int getContextUsage() override
        {
            return n_past_;
        }
        int countTokens(const std::string &text) override
        {
            // llama_tokenize() with buffer NULL, length 0 will be return negative number,
            // The absolute value is the number of tokens required.
            return -llama_tokenize(vocab_, text.c_str(), text.size(), NULL, 0, true, true);
        }

        EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) override
        {
            if (n_gpu_layers > 0 && !llama_supports_gpu_offload())
                return EngineResult::failed("loadModel.llama_supports_gpu_offload> Unsupport GPU offload in this computer");

            llama_model_params mp = llama_model_default_params();
            mp.n_gpu_layers = n_gpu_layers;
            model_ = llama_model_load_from_file(path.c_str(), mp);
            if (!model_)
                return EngineResult::failed("loadModel.llama_model_load_from_file> with path: " + path);
            //
            // const char *jinja_template_source = llama_model_chat_template(model_, /*name=*/nullptr);
            tmpl_ = createSmollm3Template();

            vocab_ = llama_model_get_vocab(model_);

            llama_context_params cp = llama_context_default_params();
            cp.n_ctx = n_ctx;
            unsigned int total = std::thread::hardware_concurrency();
            if (total == 0)
                total = 4;
            cp.n_threads_batch = total;         // Prefill — càng nhiều càng tốt
            cp.n_threads = std::min(total, 8u); // Decode — thường 4-8 là đủ, nhiều hơn ít lợi ích
            cp.no_perf = false;                 // Enable performance counters

            ctx_ = llama_init_from_model(model_, cp);
            if (!ctx_)
                return EngineResult::failed("loadModelllama_init_from_model>");

            // auto sp = llama_sampler_chain_default_params();
            // sp.no_perf = false;
            // sampler_ = llama_sampler_chain_init(sp);
            // llama_sampler_chain_add(sampler_, llama_sampler_init_greedy());
            return setTurnGrammar("", {}, false);
        }

        EngineResult trimContext(int n_tokens_to_remove) override
        {
            if (n_tokens_to_remove <= 0 || n_tokens_to_remove > n_past_ - n_keep_)
                return EngineResult::ok();

            llama_memory_t mem = llama_get_memory(ctx_);
            // Delete behind system prompt (n_keep_)
            bool removed = llama_memory_seq_rm(mem, /*seq_id=*/0,
                                               /*p0=*/n_keep_,
                                               /*p1=*/n_keep_ + n_tokens_to_remove);
            if (!removed)
                return EngineResult::failed("trimContext.llama_memory_seq_rm> with n_tokens_to_remove:" + std::to_string(n_tokens_to_remove));
            llama_memory_seq_add(mem, /*seq_id=*/0,
                                 /*p0=*/n_keep_ + n_tokens_to_remove,
                                 /*p1=*/-1,
                                 /*delta=*/-n_tokens_to_remove);
            n_past_ -= n_tokens_to_remove;
            return EngineResult::ok();
        }

        EngineResult applyChatTemplate(const std::vector<ChatMessage> &messages, bool add_generation_prompt, bool enable_thinking) override
        {
            return feedTokens(tmpl_->render(messages, add_generation_prompt, enable_thinking));
        }

        EngineResult applySystemTemplate(const std::string &system_prompt, const std::vector<std::string> &tools, bool enable_thinking) override
        {
            // TODO: system prompt tool
            EngineResult rsl = feedTokens(tmpl_->renderSystem(system_prompt, tools, enable_thinking));
            n_keep_ = n_past_;
            return rsl;
        }

        EngineResult generate(int max_tokens) override
        {
            return generateStream(max_tokens, [](const std::string &)
                                  { return true; });
        }

        EngineResult generateStream(int max_tokens, const StreamCallback &on_token) override
        {
            std::string result;

            for (int i = 0; i < max_tokens; i++)
            {
                llama_token new_id = llama_sampler_sample(sampler_, ctx_, -1); // Predict the next word
                // llama_vocab_bos,llama_vocab_eos,llama_vocab_eot for feature reason stop
                if (llama_vocab_is_eog(vocab_, new_id))
                    break; // End generate

                char buf[128];
                int n = llama_token_to_piece(vocab_, new_id, buf, sizeof(buf), 0, true); // Token to text
                if (n < 0)
                    return EngineResult::failed("generateStream.llama_token_to_piece>");

                std::string piece(buf, n);
                result.append(piece);
                // Callback with new token. When callback return false,
                // that mean caller need stop (example user click stop) -> exit loop.
                if (!on_token(piece))
                    break;

                llama_batch batch = llama_batch_get_one(&new_id, 1);
                if (llama_decode(ctx_, batch))
                    return EngineResult::failed("generateStream.llama_batch_get_one> with cycle: " + std::to_string(i));
                n_past_ += 1;
            }
            return EngineResult::ok(result);
        }

        EngineResult feedTokens(const std::string &text) override
        {
            if (text.empty())
                return EngineResult::ok();

            int n = countTokens(text);
            std::vector<llama_token> tokens(n);
            if (llama_tokenize(vocab_, text.c_str(), text.size(), tokens.data(), tokens.size(), n_past_ == 0, true) < 0)
                return EngineResult::failed("feedTokens.llama_tokenize>");

            llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
            int decode_rsl = llama_decode(ctx_, batch);
            if (decode_rsl)
                return EngineResult::failed("feedTokens.llama_decode> with code: " + std::to_string(decode_rsl));

            n_past_ += (int)tokens.size();
            return EngineResult::ok();
        }

        // GRAMMAR
        EngineResult setTurnGrammar(std::string gbnf, std::vector<std::string> triggers, bool eager) override
        {
            llama_sampler_chain_params cp = llama_sampler_chain_default_params();
            llama_sampler *chain = llama_sampler_chain_init(cp);
            if (chain == nullptr)
                return EngineResult::failed("setTurnSampler.llama_sampler_chain_init> Can not create chain");
            //
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
                    return EngineResult::failed("setTurnSampler.llama_sampler_init_grammar_lazy_patterns> Failed to build grammar sampler");
                }
                llama_sampler_chain_add(chain, grmr);
            }
            //
            llama_sampler_chain_add(chain, llama_sampler_init_top_k(40));
            llama_sampler_chain_add(chain, llama_sampler_init_temp(0.7f));
            llama_sampler_chain_add(chain, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
            // Remove old sampler
            if (sampler_ != nullptr)
                llama_sampler_free(sampler_);
            sampler_ = chain;
            return EngineResult::ok();
        }
        EngineResult setToolCallGrammar(const std::string &arguments_schema, const std::string &trigger) override
        {
            return setTurnGrammar(buildGbnfToolCall(arguments_schema, trigger), {trigger}, false);
        }
        EngineResult setAllowedRoutes(const std::vector<std::string> &routes) override
        {
            return setTurnGrammar(buildGbnfPlainEnum(routes), {}, true); // eager, không cần trigger
        }
        EngineResult setBooleanOutput() override
        {
            return setTurnGrammar(buildGbnfBoolean(), {}, true);
        }
        EngineResult setJsonSchema(const std::string &schema_json) override
        {
            return setTurnGrammar(buildGbnfJsonSchema(schema_json), {}, true);
        }

    private:
        llama_model *model_ = nullptr;
        llama_context *ctx_ = nullptr;
        llama_sampler *sampler_ = nullptr;
        const llama_vocab *vocab_ = nullptr;
        std::unique_ptr<ChatTemplate> tmpl_;
        int n_past_ = 0;
        int n_keep_ = 0;
    };
} // namespace

// Implementation of factory function
std::unique_ptr<Engine> createLlamaEngine()
{
    return std::make_unique<LlamaEngine>();
}