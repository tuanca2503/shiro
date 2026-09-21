#include <llama.h>
#include <memory>
#include <vector>

#include "context/sequence-policy.h"




namespace shiro
{
    namespace
    {
        constexpr llama_seq_id kPromptSeq = 0;

        // Ephemeral: seq 0 giữ system prompt gốc.
        // Seq làm việc = copy prefix từ seq 0, xong cắt về n_keep.
        class EphemeralSeq : public SequencePolicy
        {
        public:
            EphemeralSeq(llama_context *ctx, size_t n_seq) : SequencePolicy(ctx, n_seq) {}

            void acquire(llama_seq_id id) override
            {
                auto *mem = llama_get_memory(ctx_);
                const int32_t keep = n_keep_[kPromptSeq];

                llama_memory_seq_rm(mem, id, -1, -1);              // sạch seq đích
                llama_memory_seq_cp(mem, kPromptSeq, id, 0, keep); // copy prefix
                n_past_[id] = keep;
                n_keep_[id] = keep;
            }

            void release(llama_seq_id id) override { clear(id); }

            void clear(llama_seq_id id) override
            {
                auto *mem = llama_get_memory(ctx_);
                llama_memory_seq_rm(mem, id, n_keep_[id], -1); // xoá user/assistant, giữ prefix
                n_past_[id] = n_keep_[id];
            }
        };

        // Persistent: mỗi seq giữ KV của riêng nó qua nhiều turn.
        class PersistentSeq : public SequencePolicy
        {
        public:
            PersistentSeq(llama_context *ctx, size_t n_seq) : SequencePolicy(ctx, n_seq) {}

            void acquire(llama_seq_id) override {} // giữ nguyên KV cũ

            void release(llama_seq_id) override {} // KV là cache, không dọn

            void clear(llama_seq_id id) override
            {
                // Evict: xoá sạch cả prefix, cần nạp lại system prompt sau
                auto *mem = llama_get_memory(ctx_);
                llama_memory_seq_rm(mem, id, -1, -1);
                n_past_[id] = 0;
                n_keep_[id] = 0;
            }
        };
    } // namespace

    std::unique_ptr<SequencePolicy> makeSequencePolicy(
        PolicyType type, llama_context *ctx, size_t n_seq)
    {
        switch (type)
        {
        case PolicyType::Ephemeral:
            return std::make_unique<EphemeralSeq>(ctx, n_seq);
        case PolicyType::Persistent:
            return std::make_unique<PersistentSeq>(ctx, n_seq);
        }
        return nullptr;
    }
}