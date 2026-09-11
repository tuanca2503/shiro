#pragma once
#include <string>

enum class Role
{
    System,
    User,
    Assistant,
    Tool
};

inline const char *roleToString(Role role)
{
    switch (role)
    {
    case Role::System:
        return "system";
    case Role::User:
        return "user";
    case Role::Assistant:
        return "assistant";
    case Role::Tool:
        return "tool";
    }
    return "user"; // fallback,
}

struct ChatMessage
{
    Role role;
    std::string content;
    ChatMessage(Role r, std::string c) : role(r), content(std::move(c)) {}
};
