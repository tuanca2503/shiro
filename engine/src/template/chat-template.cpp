#include <ctime>
#include "chat-template.h"

namespace shiro
{
    namespace
    {
        class SmolLM3Template final : public ChatTemplate
        {
        public:
            std::string buildSystemPrompt(const std::string &systemMessage, const std::vector<std::string> &tools, bool enable_thinking) override
            {
                std::string customInstructions = systemMessage;
                if (findCommand(customInstructions, "/no_think") != std::string::npos)
                {
                    enable_thinking = false;
                    customInstructions = removeAll(customInstructions, "/no_think");
                }
                else if (findCommand(customInstructions, "/think") != std::string::npos)
                {
                    enable_thinking = true;
                    customInstructions = removeAll(customInstructions, "/think");
                }
                if (findCommand(customInstructions, "/system_override") != std::string::npos)
                {
                    customInstructions = removeAll(customInstructions, "/system_override");
                    return customInstructions;
                }
                rtrim(customInstructions);

                std::string result;
                result.reserve(2048);
                result.append("## Metadata\n\n")
                    .append("Knowledge Cutoff Date: June 2025\n")
                    .append("Today Date: ")
                    .append(formatTodayDate())
                    .append("\n")
                    .append("Reasoning Mode: ")
                    .append(enable_thinking ? "/think\n\n" : "/no_think\n\n")
                    .append("## Custom Instructions\n\n");

                if (!customInstructions.empty())
                    result.append(customInstructions).append("\n\n");
                else if (enable_thinking)
                    result.append(kDefaultInstructionsThink);
                else
                    result.append(kDefaultInstructionsNoThink);
                //
                if (!tools.empty())
                {
                    result.append("### Tools\n\n")
                        .append("You may call one or more functions to assist with the user query.\n")
                        .append("You are provided with function signatures within <tools></tools> XML tags:\n\n<tools>\n");

                    for (const auto &tool : tools)
                        result.append(tool).append("\n");

                    result.append("</tools>\n\n")
                        .append("For each function call, return a json object with function name and arguments "
                                "within <tool_call></tool_call> XML tags:\n<tool_call>\n"
                                "{\"name\": <function-name>, \"arguments\": <args-json-object>}\n</tool_call>\n\n");
                }
                return result;
            }

            std::string render(const std::vector<ChatMessage> &messages, bool add_generation_prompt, bool enable_thinking) override
            {
                std::string result;
                // 1. History
                for (const auto &message : messages)
                {
                    result.append("<|im_start|>")
                        .append(roleToString(message.role))
                        .append("\n")
                        .append(message.content)
                        .append("<|im_end|>\n");
                }
                // 2. Generation prompt
                if (add_generation_prompt)
                {
                    result.append("<|im_start|>assistant\n");
                    if (!enable_thinking)
                        result.append("<think>\n\n</think>\n");
                }
                return result;
            }

        private:
            static constexpr std::string_view kDefaultInstructionsThink =
                "You are a helpful AI assistant named SmolLM, trained by Hugging Face. "
                "Your role as an assistant involves thoroughly exploring questions through a "
                "systematic thinking process before providing the final precise and accurate "
                "solutions. This requires engaging in a comprehensive cycle of analysis, "
                "summarizing, exploration, reassessment, reflection, backtracking, and "
                "iteration to develop well-considered thinking process. Please structure your "
                "response into two main sections: Thought and Solution using the specified "
                "format: <think> Thought section </think> Solution section. In the Thought "
                "section, detail your reasoning process in steps. Each step should include "
                "detailed considerations such as analysing questions, summarizing relevant "
                "findings, brainstorming new ideas, verifying the accuracy of the current "
                "steps, refining any errors, and revisiting previous steps. In the Solution "
                "section, based on various attempts, explorations, and reflections from the "
                "Thought section, systematically present the final solution that you deem "
                "correct. The Solution section should be logical, accurate, and concise and "
                "detail necessary steps needed to reach the conclusion.\n\n";

            static constexpr std::string_view kDefaultInstructionsNoThink =
                "You are a helpful AI assistant named SmolLM, trained by Hugging Face.\n\n";

            std::string formatTodayDate()
            {
                std::time_t t = std::time(nullptr);
                std::tm tmBuf{};
#if defined(_WIN32)
                localtime_s(&tmBuf, &t);
#else
                localtime_r(&t, &tmBuf);
#endif
                char buf[64];
                std::strftime(buf, sizeof(buf), "%d %B %Y", &tmBuf); // "12 September 2026"
                return std::string(buf);
            }
        };

        // Unknow template

        class UnknowTemplate final : public ChatTemplate
        {
        public:
            std::string buildSystemPrompt(const std::string &systemMessage, const std::vector<std::string> & /*tools*/, bool /*enable_thinking*/) override
            {
                std::string result;
                result.reserve(2048);
                result.append("[SYSTEM NOTICE - TEMPORARY PROMPT]\n")
                    .append("This is a fallback prompt generated by an UNKNOWN chat template. ")
                    .append("The application does not have a template written specifically for the model you are running on, ")
                    .append("so the conversation format may be incorrect.\n")
                    .append("If you notice anything abnormal (the conversation format seems broken, ")
                    .append("you cannot follow the instructions, tool calls do not work, or your output is garbled), ")
                    .append("you MUST tell the user, in the user's language, that: ")
                    .append("\"This template may not be supported for this model. ")
                    .append("Please use or add a template that matches this model.\"\n")
                    .append("Otherwise, continue to help the user as normally as possible.\n\n");
                if (!systemMessage.empty())
                    result.append("[INSTRUCTIONS]\n")
                        .append(systemMessage)
                        .append("\n\n");
                return result;
            }

            std::string render(const std::vector<ChatMessage> &messages, bool add_generation_prompt, bool /*enable_thinking*/) override
            {
                std::string result;
                // 1. History
                for (const auto &message : messages)
                {
                    result.append("<|im_start|>")
                        .append(roleToString(message.role))
                        .append("\n")
                        .append(message.content)
                        .append("<|im_end|>\n");
                }
                // 2. Generation prompt
                if (add_generation_prompt)
                    result.append("<|im_start|>assistant\n");
                return result;
            }
        };
    }

    std::unique_ptr<ChatTemplate> makeChatTemplate(ChatTemplateType type)
    {
        switch (type)
        {
        case ChatTemplateType::SmolLM3:
            return std::make_unique<SmolLM3Template>();
        default:
            return std::make_unique<UnknowTemplate>();
        }
    }

} // namespace
