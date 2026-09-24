#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chat-types.h"

namespace shiro
{
    enum class ChatTemplateType
    {
        Unknown,
        SmolLM3,
    };
    //
    class ChatTemplate
    {
        // Utils maybe split to StringUtils.cpp
    protected:
        static std::string removeAll(std::string s, const std::string &needle)
        {
            if (needle.empty())
                return s;

            size_t pos = 0;
            while ((pos = s.find(needle, pos)) != std::string::npos)
                s.erase(pos, needle.length());

            return s;
        }
        static void rtrim(std::string &s)
        {
            while (!s.empty() &&
                   std::isspace(static_cast<unsigned char>(s.back())))
                s.pop_back();
        }

        static size_t findCommand(const std::string &s, const std::string &cmd)
        {
            size_t pos = 0;
            while ((pos = s.find(cmd, pos)) != std::string::npos)
            {
                size_t end = pos + cmd.size();
                bool startOk = (pos == 0) || std::isspace(static_cast<unsigned char>(s[pos - 1]));
                bool endOk = (end == s.size()) || std::isspace(static_cast<unsigned char>(s[end]));
                if (startOk && endOk)
                    return pos;
                pos = end;
            }
            return std::string::npos;
        }

    public:
        virtual ~ChatTemplate() = default;
        virtual std::string buildSystemPrompt(const std::string &string = "", const std::vector<std::string> &tools = {}, bool enable_thinking = false) = 0;
        virtual std::string render(const std::vector<ChatMessage> &messages, bool add_generation_prompt = false, bool enable_thinking = false) = 0;
    };

    std::unique_ptr<ChatTemplate> makeChatTemplate(ChatTemplateType type);
}