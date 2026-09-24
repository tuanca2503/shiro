#pragma once
namespace shiro
{
    enum class PolicyType : uint8_t
    {
        // Chạy xong → clear phần user/assistant → giữ lại system prompt → trả pool
        // về group. Không gắn với một caller cụ thể.
        Ephemeral,

        // Gắn với một logical identity (conversation/user) qua ConversationManager.
        // KV chỉ là cache: có thể bị clear hoặc evict bất kỳ lúc nào, lịch sử thật
        // nằm ở tầng server.
        Persistent,
    };
}
