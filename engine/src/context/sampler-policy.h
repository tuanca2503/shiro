#pragma once

#include "context/policy-type.h"

namespace shiro
{
    class SamplerPolicy
    {
    public:
        virtual ~SamplerPolicy() = default;

        // Sampler cho seq này (dùng cho generate)
        virtual llama_sampler *acquire(llama_seq_id seq_id) = 0;

        // Đặt sampler mới cho turn (grammar...)
        virtual void set(llama_seq_id seq_id, llama_sampler *chain) = 0;

        // Sau khi generate xong một turn
        virtual void release(llama_seq_id seq_id) = 0;
    };

    std::unique_ptr<SamplerPolicy> makeSamplerPolicy(PolicyType type, size_t n_seq);
}