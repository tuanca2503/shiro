#pragma once

#include <memory>
#include <vector>

#include "policy-type.h"
#include <iostream>

namespace shiro
{
    class SequencePolicy
    {
    public:
        virtual ~SequencePolicy() = default;
        virtual void acquire(int32_t seq_id) = 0;
        virtual void release(int32_t seq_id) = 0;
        virtual void clear(int32_t seq_id) = 0;

        int32_t nPast(int32_t id) const { return n_past_[id]; }
        bool inUse(int32_t id) const { return n_past_[id] > n_keep_[id]; }
        void advance(int32_t id, int32_t n) { n_past_[id] += n; }
        void freeze(int32_t id)
        {
            if (n_keep_[id] == 0)
                n_keep_[id] = n_past_[id];
        }
        void unfreeze(int32_t id)
        {
            n_keep_[id] = 0;
        }

    protected:
        SequencePolicy(llama_context *ctx, uint32_t n_seq)
            : ctx_(ctx), n_past_(n_seq, 0), n_keep_(n_seq, 0) {}

        llama_context *ctx_;
        std::vector<int32_t> n_past_;
        std::vector<int32_t> n_keep_; // số token prefix (system prompt) được giữ
    };

    std::unique_ptr<SequencePolicy> makeSequencePolicy(PolicyType type, llama_context *ctx, size_t n_seq);
}