#pragma once
#include <string>

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
};

