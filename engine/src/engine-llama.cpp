#include <string>
#include <vector>
#include <thread>
#include <llama.h>
#include "engine.h"

namespace
{
    class LlamaEngine : public Engine
    {
    public:
        ~LlamaEngine() override
        {
            if (sampler_)
                llama_sampler_free(sampler_);
            if (ctx_)
                llama_free(ctx_);
            if (model_)
                llama_model_free(model_);
        }

        EngineResult loadModel(const std::string &path, int n_gpu_layers, int n_ctx) override
        {
            // path: file model .gguf
            // n_gpu_layers: number of layer u want to using in gpu (99 = full)
            // n_ctx: number of context chat (4096 or higher if you want longer chat)
            ggml_backend_load_all(); // Init backend
            llama_model_params mp = llama_model_default_params();
            mp.n_gpu_layers = n_gpu_layers;
            model_ = llama_model_load_from_file(path.c_str(), mp);
            if (!model_)
                return EngineResult::failed("loadModel.llama_model_load_from_file> with path: " + path);

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

            auto sp = llama_sampler_chain_default_params();
            sp.no_perf = false;
            sampler_ = llama_sampler_chain_init(sp);
            llama_sampler_chain_add(sampler_, llama_sampler_init_greedy());

            return EngineResult::ok();
        }

        EngineResult generate(const std::string &prompt, int max_tokens) override
        {
            // prepare batch
            std::string result;
            std::string full_prompt = applyTemplate(prompt);
            int n_prompt = -llama_tokenize(vocab_, full_prompt.c_str(), full_prompt.size(), NULL, 0, true, true);
            std::vector<llama_token> tokens(n_prompt);
            if (llama_tokenize(vocab_, full_prompt.c_str(), full_prompt.size(), tokens.data(), tokens.size(), true, true))
                return EngineResult::failed("generate.llama_tokenize> with full_prompt: " + full_prompt);

            llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
            //
            for (int i = 0; i < max_tokens; i++)
            {
                int decode_rsl = llama_decode(ctx_, batch);
                if (decode_rsl)
                    return EngineResult::failed("generate.llama_decode> with code: " + std::to_string(decode_rsl));

                llama_token new_id = llama_sampler_sample(sampler_, ctx_, -1); // Predict the next word
                if (llama_vocab_is_eog(vocab_, new_id))
                    break; // End generate

                char buf[128];
                int n = llama_token_to_piece(vocab_, new_id, buf, sizeof(buf), 0, true); // Token to text
                if (n < 0)
                    return EngineResult::failed("generate.llama_token_to_piece>");

                result.append(buf, n);
                batch = llama_batch_get_one(&new_id, 1);
            }
            return EngineResult::ok(result);
        }

    private:
        llama_model *model_ = nullptr;
        llama_context *ctx_ = nullptr;
        const llama_vocab *vocab_ = nullptr;
        llama_sampler *sampler_ = nullptr;
        //
        std::string applyTemplate(const std::string &prompt)
        {
            const char *tmpl = llama_model_chat_template(model_, nullptr);

            if (tmpl == nullptr)
            {
                // Model không có template -> fallback tự viết tay
                return "<|im_start|>user\n" + prompt + "<|im_end|>\n<|im_start|>assistant\n";
            }

            llama_chat_message msg = {"user", prompt.c_str()};
            std::vector<char> formatted(prompt.size() * 2 + 256);
            int32_t n = llama_chat_apply_template(tmpl, &msg, 1, true, formatted.data(), formatted.size());

            if (n > (int32_t)formatted.size())
            {
                formatted.resize(n);
                n = llama_chat_apply_template(tmpl, &msg, 1, true, formatted.data(), formatted.size());
            }

            return std::string(formatted.data(), n);
        }
    };
}

// Implementation của factory function
std::unique_ptr<Engine> createLlamaEngine()
{
    return std::make_unique<LlamaEngine>();
}