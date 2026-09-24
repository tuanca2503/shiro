#pragma once
#include <string>

namespace shiro
{
    enum class ChatRole
    {
        System,
        User,
        Assistant,
        Tool
    };

    inline const char *roleToString(ChatRole role)
    {
        switch (role)
        {
        case ChatRole::System:
            return "system";
        case ChatRole::User:
            return "user";
        case ChatRole::Assistant:
            return "assistant";
        case ChatRole::Tool:
            return "tool";
        }
        return "user"; // fallback,
    }

    struct ChatMessage
    {
        ChatRole role;
        std::string content;
        ChatMessage(ChatRole r, std::string c) : role(r), content(std::move(c)) {}

        static ChatMessage from_system(std::string content)
        {
            return ChatMessage(ChatRole::System, std::move(content));
        }
        static ChatMessage from_user(std::string content)
        {
            return ChatMessage(ChatRole::User, std::move(content));
        }

        static ChatMessage from_assistant(std::string content)
        {
            return ChatMessage(ChatRole::Assistant, std::move(content));
        }

        static ChatMessage from_tool(std::string content)
        {
            return ChatMessage(ChatRole::Tool, std::move(content));
        }
    };

}
