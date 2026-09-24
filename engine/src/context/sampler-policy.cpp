#include <llama.h>

#include "sampler-policy.h"

namespace shiro
{
    namespace
    {
        // Một sampler dùng chung, dùng xong reset
        class EphemeralPolicy : public SamplerPolicy
        {
        public:
            ~EphemeralPolicy() override
            {
                if (sampler_)
                    llama_sampler_free(sampler_);
            }

            llama_sampler *acquire(llama_seq_id) override { return sampler_; }

            void set(llama_seq_id, llama_sampler *chain) override
            {
                if (sampler_)
                    llama_sampler_free(sampler_);
                sampler_ = chain;
            }

            void release(llama_seq_id) override
            {
                if (sampler_)
                    llama_sampler_reset(sampler_);
            }

        private:
            llama_sampler *sampler_ = nullptr;
        };

        // Mỗi seq một sampler, giữ qua nhiều turn
        class PersistentPolicy : public SamplerPolicy
        {
        public:
            explicit PersistentPolicy(size_t n_seq) : samplers_(n_seq, nullptr) {}

            ~PersistentPolicy() override
            {
                for (auto *s : samplers_)
                    if (s)
                        llama_sampler_free(s);
            }

            llama_sampler *acquire(llama_seq_id id) override { return samplers_[id]; }

            void set(llama_seq_id id, llama_sampler *chain) override
            {
                if (samplers_[id])
                    llama_sampler_free(samplers_[id]);
                samplers_[id] = chain;
            }

            void release(llama_seq_id) override {} // giữ state cho turn sau

        private:
            std::vector<llama_sampler *> samplers_;
        };
    } // namespace

    std::unique_ptr<SamplerPolicy> makeSamplerPolicy(PolicyType type, size_t n_seq)
    {
        switch (type)
        {
        case PolicyType::Ephemeral:
            return std::make_unique<EphemeralPolicy>();
        case PolicyType::Persistent:
            return std::make_unique<PersistentPolicy>(n_seq);
        }
        return nullptr;
    }
}