#pragma once

// context-manager.h — shiro.cpp
//
// Sở hữu mọi ContextPool, gom theo profile_id (KHÔNG gom theo n_ctx trần —
// xem docs/context-layer-notes.md §1.3 lý do). Vai trò "ContextGroup" của
// plan gốc được gộp thẳng vào đây thay vì tách file riêng: một group chỉ là
// "danh sách pool của một profile", không có state nào khác đáng một class
// riêng — nếu context-group.h đã tồn tại với nội dung khác, thay phần thân
// loadOrCreateGroup()/tryAcquireLocked() bằng lời gọi sang đó, chữ ký public
// của ContextManager không cần đổi.
//
// Hai đường dùng:
//   - Ephemeral (classifier, planner): acquire(profile_id) trả ContextHandle
//     RAII, destructor tự release, Agent không tự tay release.
//   - Persistent (conversation): acquireSlot()/releaseSlot() là hai callback
//     tiêm thẳng vào ConversationManager — xem conversation-manager.h. Không
//     bao giờ chờ; ConversationManager tự quyết định evict/reject khi hết
//     pool, ContextManager chỉ báo có hay không.

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "context.h"

// struct llama_model;

namespace shiro
{

    // class ContextManager;

    // class ContextHandle
    // {
    // public:
    //     ContextHandle() = default;
    //     ContextHandle(ContextHandle &&other) noexcept { *this = std::move(other); }
    //     ContextHandle &operator=(ContextHandle &&other) noexcept;
    //     ContextHandle(const ContextHandle &) = delete;
    //     ContextHandle &operator=(const ContextHandle &) = delete;
    //     ~ContextHandle();

    //     bool valid() const { return pool_ != nullptr; }
    //     explicit operator bool() const { return valid(); }

    //     ContextPool *operator->() { return pool_; }
    //     const ContextPool *operator->() const { return pool_; }
    //     ContextPool &operator*() { return *pool_; }

    // private:
    //     friend class ContextManager;
    //     ContextHandle(ContextManager *mgr, ContextPool *pool) : mgr_(mgr), pool_(pool) {}
    //     void release();

    //     ContextManager *mgr_ = nullptr;
    //     ContextPool *pool_ = nullptr;
    // };

    class ContextManager
    {
    public:
        ContextManager(llama_model *model, int32_t n_threads);
        ContextManager(const ContextManager &) = delete;
        ContextManager &operator=(const ContextManager &) = delete;

        Context *create(uint32_t n_ctx,
                        uint32_t n_seq_max,
                        bool kv_unified,
                        PolicyType lifecycle,
                        int max_tokens);

        // // Đường ephemeral. Chờ/từ chối theo AcquirePolicy của profile — Evict
        // // không hợp lệ ở đây, ContextProfileRegistry::add() đã chặn Evict cho
        // // lifecycle Ephemeral từ lúc đăng ký. Handle rỗng (valid() == false) nếu
        // // profile không tồn tại, hoặc Reject/timeout khi hết pool.
        // ContextHandle acquire(const std::string &profile_id);

        // // Đường persistent — hai callback cho ConversationManager. Không chờ,
        // // không tự evict: trả false ngay khi hết pool rảnh và chưa đạt max_pools.
        // bool acquireSlot(const ContextProfile &profile, RuntimeBinding &out);
        // void releaseSlot(const RuntimeBinding &binding);

        // struct GroupStats
        // {
        //     uint32_t total = 0, busy = 0, ready = 0;
        // };
        // GroupStats statsFor(const std::string &profile_id) const;

        private:
            std::vector<llama_context *> ctxs_;

            friend class ContextHandle;
            // void releasePool(ContextPool *pool);

            // Giữ mu_ khi gọi (caller lock). Trả nullptr nếu không có pool rảnh và
            // group đã chạm max_pools của profile.
            // ContextPool *tryAcquireLocked(const ContextProfile &profile);

            llama_model *model_;
            int32_t n_threads_;
            std::vector<std::unique_ptr<Context>> contexts_;

            // ContextPool::PrefillFn prefill_;

            mutable std::mutex mu_;
            std::condition_variable cv_;

            // profile_id -> các pool của group đó. unique_ptr giữ địa chỉ ContextPool
            // ổn định qua mọi lần vector reallocate (chỉ con trỏ bên trong unique_ptr
            // di chuyển, không phải đối tượng nó trỏ tới).
            // std::unordered_map<std::string, std::vector<std::unique_ptr<ContextPool>>> groups_;

            // pool_id -> con trỏ thô vào pool nằm trong groups_. Cần cho releaseSlot()
            // vì RuntimeBinding không mang profile_id, chỉ mang pool_id.
            // std::unordered_map<uint32_t, ContextPool *> by_pool_id_;

            uint32_t next_pool_id_ = 1;
    };

} // namespace shiro