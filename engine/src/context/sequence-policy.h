#pragma once
#include "context/policy-type.h"

namespace shiro
{
    class SequencePolicy
    {
    public:
        virtual ~SequencePolicy() = default;
        // Chuẩn bị seq trước khi dùng
        virtual void acquire(llama_seq_id seq_id) = 0;
        // Sau khi xong một turn
        virtual void release(llama_seq_id seq_id) = 0;
        // Xoá KV của seq
        virtual void clear(llama_seq_id seq_id) = 0;

        int32_t nPast(llama_seq_id id) const { return n_past_[id]; }
        void advance(llama_seq_id id, int32_t n) { n_past_[id] += n; }
        void freeze(llama_seq_id id)
        {
            if (n_keep_[id] == 0)
                n_keep_[id] = n_past_[id];
        }

    protected:
        SequencePolicy(llama_context *ctx, size_t n_seq)
            : ctx_(ctx), n_past_(n_seq + 1, 0), n_keep_(n_seq + 1, 0) {}

        llama_context *ctx_;
        std::vector<int32_t> n_past_;
        std::vector<int32_t> n_keep_; // số token prefix (system prompt) được giữ
    };

    std::unique_ptr<SequencePolicy> makeSequencePolicy(PolicyType type, llama_context *ctx, size_t n_seq);
}