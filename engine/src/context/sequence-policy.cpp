#include <llama.h>
#include "sequence-policy.h"

namespace shiro
{
    namespace
    {
        constexpr int32_t kPromptSeq = 0;

        // Ephemeral: seq 0 giữ system prompt gốc.
        // Seq làm việc = copy prefix từ seq 0, xong cắt về n_keep.
        class EphemeralSeq : public SequencePolicy
        {
        public:
            EphemeralSeq(llama_context *ctx, uint32_t n_seq) : SequencePolicy(ctx, n_seq) {}

            void acquire(int32_t id) override
            {
                auto *mem = llama_get_memory(ctx_);
                const int32_t keep = n_keep_[kPromptSeq];
                // in here seq 0 has prompt but want to set prompt again or using it
                // case 1 seq 0
                if (id == kPromptSeq)
                {
                    if (keep <= 0)
                    {
                        llama_memory_seq_rm(mem, id, -1, -1);
                        n_past_[id] = 0;
                    } // when need set prompt then keep == 0 ->  remove all cache
                    return; // seq 0 no need reset cache and copy -> skip
                }
                // case 2 other seq
                llama_memory_seq_rm(mem, id, -1, -1);
                llama_memory_seq_cp(mem, kPromptSeq, id, 0, keep);
                n_past_[id] = keep;
                n_keep_[id] = keep;
            }

            void release(int32_t id) override { clear(id); }

            void clear(int32_t id) override
            {
                if (n_keep_[id] <= 0)
                    return;

                auto *mem = llama_get_memory(ctx_);
                llama_memory_seq_rm(mem, id, n_keep_[kPromptSeq], -1); // xoá user/assistant, giữ prefix
                n_past_[id] = n_keep_[id];
            }
        };

        // Persistent: mỗi seq giữ KV của riêng nó qua nhiều turn.
        class PersistentSeq : public SequencePolicy
        {
        public:
            PersistentSeq(llama_context *ctx, size_t n_seq) : SequencePolicy(ctx, n_seq) {}

            void acquire(int32_t) override {} // giữ nguyên KV cũ

            void release(int32_t) override {} // KV là cache, không dọn

            void clear(int32_t id) override
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