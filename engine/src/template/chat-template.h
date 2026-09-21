#pragma once

#include <memory>
#include <string>
#include <vector>

#include "types/chat-types.h"

namespace shiro
{
    enum class ChatTemplateType
    {
        Unknown,
        SmolLM3,
    };

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

    public:
        virtual ~ChatTemplate() = default;

        virtual std::string renderSystem(const std::string &string, const std::vector<std::string> &tools, bool enable_thinking) = 0;

        virtual std::string render(const std::vector<ChatMessage> &messages, bool add_generation_prompt, bool enable_thinking) = 0;
    };

    std::unique_ptr<ChatTemplate> createSmollm3Template();

}