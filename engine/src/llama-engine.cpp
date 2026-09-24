#include <thread>
#include <iostream>

#include <llama.h>
#include "engine.h"

#include "chat-template.h"
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
                {
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
                }

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
                if (model_)
                    llama_model_free(model_);
                llama_backend_free();
            }
            //
            Result loadModel(const std::string &path, int n_gpu_layers) override
            {
                // DEBUG
                auto t0 = std::chrono::steady_clock::now(); // <- start timer
                std::fprintf(stderr, "[DEBUG] LlamaEngine.loadModel> Start with gpu layer: %d\n", n_gpu_layers);
                // END DEBUG
                if (n_gpu_layers > 0 && !llama_supports_gpu_offload())
                    return Result::failed("loadModel.llama_supports_gpu_offload> Unsupport GPU offload in this computer");

                llama_model_params mp = llama_model_default_params();
                mp.n_gpu_layers = n_gpu_layers;
                model_ = llama_model_load_from_file(path.c_str(), mp);
                if (!model_)
                    return Result::failed("loadModel.llama_model_load_from_file> with path: " + path);
                //
                unsigned int hc = std::thread::hardware_concurrency();
                n_threads_ = hc > 0 ? static_cast<int32_t>(hc) : 4; // fallback khi hc == 0

                // DEBUG
                auto t1 = std::chrono::steady_clock::now();
                double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                std::fprintf(stderr, "[DEBUG] LlamaEngine.loadModel> in %.3f ms\n", total_ms);
                // END DEBUG
                return Result::ok();
            }
            Context *createContextEphemeral(uint32_t n_ctx, uint32_t n_seq_max, bool kv_unified, int max_tokens) override
            {
                auto context = std::unique_ptr<Context>(
                    new Context(
                        model_,
                        n_threads_,
                        n_ctx,
                        n_seq_max,
                        kv_unified,
                        PolicyType::Ephemeral,
                        max_tokens));

                if (!context->isValid())
                    return nullptr;

                Context *result = context.get();
                contexts_.push_back(std::move(context));
                return result;
            }
            Context *createContextPersistent(uint32_t n_ctx, uint32_t n_seq_max, bool kv_unified, int max_tokens) override
            {
                auto context = std::unique_ptr<Context>(
                    new Context(
                        model_,
                        n_threads_,
                        n_ctx,
                        n_seq_max,
                        kv_unified,
                        PolicyType::Persistent,
                        max_tokens));

                if (!context->isValid())
                    return nullptr;

                Context *result = context.get();
                contexts_.push_back(std::move(context));
                return result;
            }
            bool destroyContext(Context *ctx) override
            {
                if (!ctx)
                    return false;

                auto it = std::find_if(contexts_.begin(), contexts_.end(),
                                       [ctx](const std::unique_ptr<Context> &p)
                                       { return p.get() == ctx; });
                if (it == contexts_.end())
                    return false;

                contexts_.erase(it); // -> ~Context() -> llama_free(ctx_)
                return true;
            }

        private:
            llama_model *model_ = nullptr;
            int32_t n_threads_;
            std::vector<std::unique_ptr<Context>> contexts_;
        };
    } // namespace
    // Implementation of factory function
    std::unique_ptr<Engine> createLlamaEngine()
    {
        return std::make_unique<LlamaEngine>();
    }
}
