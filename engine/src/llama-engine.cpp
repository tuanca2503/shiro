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
        LlamaEngine()
        {
            llama_backend_init();
        }
        ~LlamaEngine() override
        {

            if (sampler_)
                llama_sampler_free(sampler_);
            if (ctx_)
                llama_free(ctx_);
            if (model_)
            {
                llama_model_free(model_);
            }
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

        EngineResult trimContext(int n_tokens_to_remove) override
        {
            if (!model_ || n_tokens_to_remove <= 0 || n_tokens_to_remove > n_past_ - n_keep_)
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

        EngineResult applyChatTemplate(const std::vector<ChatMessage> &messages) override
        {
            if (!model_)
                return EngineResult::failed("applyTemplate> Model is not loading");

            const char *tmpl = llama_model_chat_template(model_, /*name=*/nullptr);
            if (tmpl == nullptr)
                return EngineResult::failed("applyTemplate> Model not contain template");

            std::vector<llama_chat_message> chat_msgs;
            chat_msgs.reserve(messages.size());
            for (const auto &m : messages)
                chat_msgs.push_back({roleToString(m.role), m.content.c_str()});

            std::vector<char> buf(messages.size() * 256 + 256);
            int32_t n = llama_chat_apply_template(
                tmpl, chat_msgs.data(), chat_msgs.size(), /*add_ass=*/true,
                buf.data(), (int32_t)buf.size());

            if (n < 0)
                return EngineResult::failed("applyTemplate.llama_chat_apply_template>");

            if (n > (int32_t)buf.size())
            {
                buf.resize(n);
                n = llama_chat_apply_template(
                    tmpl, chat_msgs.data(), chat_msgs.size(), /*add_ass=*/true,
                    buf.data(), (int32_t)buf.size());
            }
            return EngineResult::ok(std::string(buf.data(), n));
        }

        EngineResult generate(const std::string &prompt, int max_tokens) override
        {
            return generateStream(prompt, max_tokens, [](const std::string &)
                                  { return true; });
        }

        EngineResult generateStream(const std::string &prompt, int max_tokens, const StreamCallback &on_token) override
        {
            if (!model_)
                return EngineResult::failed("generateStream> Model is not loading");
            // Prepare batch
            int n_prompt = countTokens(prompt);
            std::vector<llama_token> tokens(n_prompt);
            if (llama_tokenize(vocab_, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true) < 0)
                return EngineResult::failed("generateStream.llama_tokenize> with prompt: " + prompt);
            llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
            std::string result;

            for (int i = 0; i < max_tokens; i++)
            {
                int decode_rsl = llama_decode(ctx_, batch);
                if (decode_rsl)
                    return EngineResult::failed("generateStream.llama_decode> with code: " + std::to_string(decode_rsl));
                n_past_ += batch.n_tokens;
                llama_token new_id = llama_sampler_sample(sampler_, ctx_, -1); // Predict the next word
                // llama_vocab_bos,llama_vocab_eos,llama_vocab_eot for feature reason stop
                if (llama_vocab_is_eog(vocab_, new_id))
                    break; // End generate

                char buf[128];
                int n = llama_token_to_piece(vocab_, new_id, buf, sizeof(buf), 0, true); // Token to text
                if (n > 0)
                {
                    std::string piece(buf, n);
                    result.append(piece);

                    // Callback with new token. When callback return false,
                    // that mean caller need stop (example user click stop) -> exit loop.
                    if (!on_token(piece))
                        break;
                }
                batch = llama_batch_get_one(&new_id, 1);
            }
            return EngineResult::ok(result);
        }

        EngineResult setSystemPrompt(const std::string &system_prompt) override
        {
            EngineResult rsl = applyChatTemplate({ChatMessage(Role::System, system_prompt)});
            if (!rsl.success)
                return rsl;

            std::string full_prompt = rsl.text;
            int n_tokens = countTokens(full_prompt);
            std::vector<llama_token> tokens(n_tokens);
            if (llama_tokenize(vocab_, full_prompt.c_str(), full_prompt.size(), tokens.data(), tokens.size(), true, true) < 0)
                return EngineResult::failed("setSystemPrompt.llama_tokenize> with prompt: " + system_prompt);

            llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
            int decode_rsl = llama_decode(ctx_, batch);
            if (decode_rsl)
                return EngineResult::failed("setSystemPrompt.llama_decode> with code: " + std::to_string(decode_rsl));

            n_past_ = n_tokens;
            n_keep_ = n_tokens;

            return EngineResult::ok();
        }

    private:
        llama_model *model_ = nullptr;
        llama_context *ctx_ = nullptr;
        llama_sampler *sampler_ = nullptr;
        const llama_vocab *vocab_ = nullptr;
        int n_past_ = 0;
        int n_keep_ = 0;
    };
}

// Implementation of factory function
std::unique_ptr<Engine> createLlamaEngine()
{
    return std::make_unique<LlamaEngine>();
}