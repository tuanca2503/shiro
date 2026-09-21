#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <llama.h>

#include "engine.h"
#include "template/chat-template.h"
#include "template/gbnf-builder.h"

namespace shiro
{
    namespace
    {
        class LlamaEngine : public Engine
        {
        public:
            LlamaEngine()
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine> Start backend init\n");

                llama_log_set([](enum ggml_log_level level, const char *text, void *user_data)
                              {
            (void)user_data;
            if (level == GGML_LOG_LEVEL_CONT)
            {
                std::fputs(text, stderr);
                return;
            }
            if (level == GGML_LOG_LEVEL_DEBUG)
                return;
            const char *tag =
                level == GGML_LOG_LEVEL_ERROR ? "[ERROR]" :
                level == GGML_LOG_LEVEL_WARN  ? "[WARN]" :
                                                "[INFO]";

            std::fprintf(stderr, "%s llama.cpp> %s", tag, text); }, nullptr);
                // END DEBUG
                llama_backend_init();
                llama_numa_init(GGML_NUMA_STRATEGY_DISABLED); // GGML_NUMA_STRATEGY_DISTRIBUTE de tan dung dung NUMA (enable when run gpu)
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine> Backend init in %.3f ms\n", total_ms);
                // END DEBUG
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
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.clearContext> Start\n");
                // END DEBUG
                if (!ctx_)
                    return;
                llama_memory_t mem = llama_get_memory(ctx_);
                // DEBUG
                llama_pos max_pos = llama_memory_seq_pos_max(mem, /*seq_id=*/0);
                int token_count = max_pos + 1;
                // END DEBUG
                llama_memory_clear(mem, true);
                n_past_ = 0;
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine> Removed %d tokens from KV-cache in %.3f ms\n", token_count, total_ms);
                // END DEBUG
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
            //
            EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.loadModel> Start with gpu layer: %d, max token in context: %d \n", n_gpu_layers, n_ctx);
                // END DEBUG
                if (n_gpu_layers > 0 && !llama_supports_gpu_offload())
                    return EngineResult::failed("loadModel.llama_supports_gpu_offload> Unsupport GPU offload in this computer");

                llama_model_params mp = llama_model_default_params();
                mp.n_gpu_layers = n_gpu_layers;
                model_ = llama_model_load_from_file(path.c_str(), mp);
                if (!model_)
                    return EngineResult::failed("loadModel.llama_model_load_from_file> with path: " + path);

                // const char *jinja_template_source = llama_model_chat_template(model_, /*name=*/nullptr);
                tmpl_ = createSmollm3Template();
                vocab_ = llama_model_get_vocab(model_);

                llama_context_params cp = llama_context_default_params();
                cp.n_ctx = n_ctx;

                unsigned int total = std::thread::hardware_concurrency();
                if (total == 0)
                    return EngineResult::failed("loadModel.hardware_concurrency> Unsupport hardware = 0 in this computer");

                cp.n_threads_batch = total;         // Prefill — use as many threads as possible
                cp.n_threads = std::min(total, 8u); // Decode — 4–8 threads is usually sufficient; additional threads provide limited benefit
                // DEBUG
                cp.no_perf = false; // Enable performance counters
                // END DEBUG

                ctx_ = llama_init_from_model(model_, cp);
                if (!ctx_)
                    return EngineResult::failed("loadModel.llama_init_from_model>");
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.loadModel> in %.3f ms\n", total_ms);
                // END DEBUG
                return setTurnGrammar("", {}, false);
            }
            EngineResult trimContext(int n_tokens_to_remove) override
            {
                if (n_tokens_to_remove <= 0 || n_tokens_to_remove > n_past_ - n_keep_)
                    return EngineResult::ok();
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.trimContext> Start removed: %d tokens from total: %d and keep in: %d \n", n_tokens_to_remove, n_past_, n_keep_);
                // END DEBUG

                llama_memory_t mem = llama_get_memory(ctx_);
                bool removed = llama_memory_seq_rm(mem, /*seq_id=*/0, /*p0=*/n_keep_, /*p1=*/n_keep_ + n_tokens_to_remove); // Delete behind system prompt (n_keep_)
                if (!removed)
                    return EngineResult::failed("trimContext.llama_memory_seq_rm> with n_tokens_to_remove:" + std::to_string(n_tokens_to_remove));

                llama_memory_seq_add(mem, /*seq_id=*/0, /*p0=*/n_keep_ + n_tokens_to_remove, /*p1=*/-1, /*delta=*/-n_tokens_to_remove); // Data translation
                n_past_ -= n_tokens_to_remove;
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.trimContext> in %.3f ms\n", total_ms);
                // END DEBUG
                return EngineResult::ok();
            }
            EngineResult applyChatTemplate(const std::vector<ChatMessage> &messages, bool add_generation_prompt, bool enable_thinking) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.applyChatTemplate> Start\n");
                // END DEBUG
                std::string full_prompt = tmpl_->render(messages, add_generation_prompt, enable_thinking);
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.applyChatTemplate> from prompt: %.70s... in %.3f ms\n", full_prompt.c_str(), total_ms);
                // END DEBUG
                return feedTokens(full_prompt);
            }
            EngineResult applySystemTemplate(const std::string &system_prompt, const std::vector<std::string> &tools, bool enable_thinking) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.applySystemTemplate> Start\n");
                // END DEBUG
                std::string full_prompt = tmpl_->renderSystem(system_prompt, tools, enable_thinking);
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.applySystemTemplate> from prompt: %.70s... in %.3f ms\n", full_prompt.c_str(), total_ms);
                // END DEBUG
                auto rsl = feedTokens(full_prompt);
                n_keep_ = n_past_;
                return rsl;
            }
            EngineResult feedTokens(const std::string &text) override
            {
                if (text.empty())
                    return EngineResult::ok();
                int n = countTokens(text);
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.feedTokens> Start %d tokens\n", n);
                // END DEBUG
                std::vector<llama_token> tokens(n);
                if (llama_tokenize(vocab_, text.c_str(), text.size(), tokens.data(), tokens.size(), n_past_ == 0, true) < 0)
                    return EngineResult::failed("feedTokens.llama_tokenize>");
                llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
                // DEBUG
                auto t00 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.feedTokens.llama_decode> Start decode %d tokens\n", n);
                // END DEBUG
                int decode_rsl = llama_decode(ctx_, batch);
                if (decode_rsl)
                    return EngineResult::failed("feedTokens.llama_decode> with code: " + std::to_string(decode_rsl));
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t00).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.feedTokens.llama_decode> in %.3f ms\n", total_ms);
                total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.feedTokens> in %.3f ms\n", total_ms);
                // END DEBUG
                n_past_ += (int)tokens.size();
                return EngineResult::ok();
            }
            EngineResult generate(int max_tokens) override
            {
                return generateStream(max_tokens, [](const std::string &)
                                      { return true; });
            }
            EngineResult generateStream(int max_tokens, const StreamCallback &on_token) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                int temp = 0;
                std::fprintf(stderr, "[DEBUG] LlamaEngine.generateStream> Start with max token generate: %d\n", max_tokens);
                // END DEBUG

                std::string result;
                for (int i = 0; i < max_tokens; i++)
                {
                    llama_token new_id = llama_sampler_sample(sampler_, ctx_, -1); // Predict the next word
                    // llama_vocab_bos,llama_vocab_eos,llama_vocab_eot for feature return detail reason stop
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
                    {
                        llama_token eot = llama_vocab_eot(vocab_);
                        llama_token toks[2] = {new_id, eot};
                        llama_batch batch = llama_batch_get_one(toks, 2);
                        if (llama_decode(ctx_, batch) == 0)
                            n_past_ += 2;
                        break;
                    }

                    llama_batch batch = llama_batch_get_one(&new_id, 1);
                    if (llama_decode(ctx_, batch))
                        return EngineResult::failed("generateStream.llama_batch_get_one> with cycle: " + std::to_string(i));
                    n_past_ += 1;
                    // DEBUG
                    temp += 1;
                    // END DEBUG
                }
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                double ms_per_token = total_ms / temp;
                std::fprintf(
                    stderr,
                    "[DEBUG] LlamaEngine.generateStream> %.3f ms/token, total %.3f ms\n",
                    ms_per_token,
                    total_ms);
                // END DEBUG

                return EngineResult::ok(result);
            }
            // GRAMMAR
            EngineResult setToolCallGrammar(const std::string &json, const std::string &trigger) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setToolCallGrammar> Start build gbnf with trigger %s \n", trigger.c_str());
                // END DEBUG
                std::string gbnf = buildGbnfToolCall(json, trigger);
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setToolCallGrammar> %s from schema: %.70s... in  %.3f ms\n", gbnf.c_str(), json.c_str(), total_ms);
                // END DEBUG
                return setTurnGrammar(gbnf, {trigger}, false);
            }
            EngineResult setRoutesGrammar(const std::vector<std::string> &routes) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setRoutesGrammar> Start build gbnf \n");
                // END DEBUG
                std::string gbnf = buildGbnfPlainEnum(routes);
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setRoutesGrammar> %s from routes size: %zu in  %.3f ms\n", gbnf.c_str(), routes.size(), total_ms);
                // END DEBUG
                return setTurnGrammar(gbnf, {}, true); // eager, no need trigger
            }
            EngineResult setBooleanGrammar() override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setBooleanGrammar> Start build gbnf \n");
                // END DEBUG
                std::string gbnf = buildGbnfBoolean();
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setBooleanGrammar>%s in  %.3f ms\n", gbnf.c_str(), total_ms);
                // END DEBUG
                return setTurnGrammar(gbnf, {}, true);
            }
            EngineResult setJsonGrammar(const std::string &json) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setJsonGrammar> Start build gbnf \n");
                // END DEBUG
                std::string gbnf = buildGbnfJsonSchema(json);
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setJsonGrammar>%s from schema: %.70s... in  %.3f ms\n", gbnf.c_str(), json.c_str(), total_ms);
                // END DEBUG
                return setTurnGrammar(gbnf, {}, true);
            }
            EngineResult setTurnGrammar(const std::string &gbnf, const std::vector<std::string> &triggers, bool eager) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setTurnGrammar> Start\n");
                // END DEBUG
                llama_sampler_chain_params cp = llama_sampler_chain_default_params();
                // DEBUG
                cp.no_perf = true;
                // END DEBUG
                llama_sampler *chain = llama_sampler_chain_init(cp);
                if (chain == nullptr)
                    return EngineResult::failed("setTurnGrammar.llama_sampler_chain_init> Can not create chain");

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
                        return EngineResult::failed("setTurnGrammar.llama_sampler_init_grammar_lazy_patterns> Failed to build grammar sampler");
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
                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.setTurnGrammar> free and set new sampler_ in %.3f ms\n", total_ms);
                // END DEBUG
                return EngineResult::ok();
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
    }
    // Implementation of factory function
    std::unique_ptr<Engine> createLlamaEngine()
    {
        return std::make_unique<LlamaEngine>();
    }
} // namespace
