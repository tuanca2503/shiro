#include <chrono>
#include <llama.h>

#include "context/context-manager.h"

namespace shiro
{

    // ---------------------------------------------------------------- ContextHandle

    // void ContextHandle::release() {
    //     if (mgr_ != nullptr && pool_ != nullptr) mgr_->releasePool(pool_);
    //     mgr_  = nullptr;
    //     pool_ = nullptr;
    // }

    // ContextHandle::~ContextHandle() { release(); }

    // ContextHandle& ContextHandle::operator=(ContextHandle&& other) noexcept {
    //     if (this != &other) {
    //         release();
    //         mgr_        = other.mgr_;
    //         pool_       = other.pool_;
    //         other.mgr_  = nullptr;
    //         other.pool_ = nullptr;
    //     }
    //     return *this;
    // }

    // --------------------------------------------------------------- ContextManager
    /*
    LlamaEngine
        ↓
    ContextManager
        ↓
    Context
        ↓
    llama_context
        ↓
    batch,pool,v.v,seq
    */
    ContextManager::ContextManager(llama_model *model, int32_t n_threads)
        : model_(model), n_threads_(n_threads)
    {
    }

    Context *ContextManager::create(uint32_t n_ctx,
                                    uint32_t n_seq_max,
                                    bool kv_unified,
                                    PolicyType lifecycle,
                                    int max_tokens)
    {
        auto context = Context::createEphemeral(model_,
                                       n_threads_,
                                       n_ctx,
                                       n_seq_max,
                                       kv_unified,
                                       max_tokens);
        if (!context)
            return nullptr;

        Context *result = context.get();
        contexts_.push_back(std::move(context));
        return result;
    }

}

// ContextPool *ContextManager::tryAcquireLocked(const ContextProfile &profile)
// {
//     auto &pools = groups_[profile.id]; // tạo group rỗng nếu chưa có (lazy)

//     for (auto &p : pools)
//     {
//         if (!p->busy())
//         {
//             p->markBusy();
//             return p.get();
//         }
//     }

//     if (static_cast<uint32_t>(pools.size()) >= profile.max_pools)
//         return nullptr;

//     auto pool = std::make_unique<ContextPool>(model_, profile, next_pool_id_++, prefill_);
//     if (!pool->create())
//         return nullptr; // pool tự huỷ khi ra khỏi scope này

//     pool->markBusy();
//     ContextPool *raw = pool.get();
//     by_pool_id_[raw->poolId()] = raw;
//     pools.push_back(std::move(pool));
//     return raw;
// }

// ContextHandle ContextManager::acquire(const std::string &profile_id)
// {
//     const ContextProfile *profile = profiles_.find(profile_id);
//     if (profile == nullptr)
//         return ContextHandle{};

//     std::unique_lock<std::mutex> lock(mu_);

//     ContextPool *pool = tryAcquireLocked(*profile);
//     if (pool != nullptr)
//         return ContextHandle(this, pool);

//     if (profile->acquire_policy == AcquirePolicy::Reject)
//         return ContextHandle{};

//     // Còn lại là Wait (Evict đã bị ContextProfileRegistry::add() chặn cho
//     // lifecycle Ephemeral, nên không cần xử lý riêng ở đây).
//     const auto deadline = std::chrono::steady_clock::now() +
//                           std::chrono::milliseconds(profile->acquire_timeout_ms);

//     while (true)
//     {
//         if (cv_.wait_until(lock, deadline) == std::cv_status::timeout)
//         {
//             return ContextHandle{};
//         }
//         pool = tryAcquireLocked(*profile);
//         if (pool != nullptr)
//             return ContextHandle(this, pool);
//     }
// }

// bool ContextManager::acquireSlot(const ContextProfile &profile, RuntimeBinding &out)
// {
//     std::lock_guard<std::mutex> lock(mu_);

//     ContextPool *pool = tryAcquireLocked(profile);
//     if (pool == nullptr)
//         return false;

//     out.pool_id = pool->poolId();
//     out.seq_id = pool->seqId();
//     out.n_past = pool->nPast();
//     return true;
// }

// void ContextManager::releaseSlot(const RuntimeBinding &binding)
// {
//     ContextPool *pool = nullptr;
//     {
//         std::lock_guard<std::mutex> lock(mu_);
//         auto it = by_pool_id_.find(binding.pool_id);
//         if (it == by_pool_id_.end())
//             return;
//         pool = it->second;
//     }
//     releasePool(pool);
// }

// void ContextManager::releasePool(ContextPool *pool)
// {
//     if (pool == nullptr)
//         return;

//     // Reset KV KHÔNG giữ mu_ — đây là việc tốn thời gian (có thể phải
//     // prefill lại nếu model không xoá được một phần). Pool vẫn ở trạng thái
//     // Busy suốt lúc này nên không ai khác lấy nhầm nó giữa chừng.
//     pool->resetToSystemPrompt();

//     {
//         std::lock_guard<std::mutex> lock(mu_);
//         pool->markReady();
//     }
//     cv_.notify_one();
// }

// ContextManager::GroupStats ContextManager::statsFor(const std::string &profile_id) const
// {
//     std::lock_guard<std::mutex> lock(mu_);

//     GroupStats stats;
//     auto it = groups_.find(profile_id);
//     if (it == groups_.end())
//         return stats;

//     for (const auto &p : it->second)
//     {
//         ++stats.total;
//         if (p->busy())
//             ++stats.busy;
//         else
//             ++stats.ready;
//     }
//     return stats;
// }

// } // namespace shiro