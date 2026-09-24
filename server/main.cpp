// daemon/main.cpp
// File nay KHONG include llama.h, khong biet class LlamaEngine ton tai.
// Chi biet duy nhat "engine.h" va ham factory createLlamaEngine().
#include <iostream>
#include "engine.h"
#include "chat-template.h"

using namespace shiro;

int main(int argc, char **argv)
{
    // DEBUG
    //  auto a = makeChatTemplate(ChatTemplateType::Unknown);

    // std::string b = a->buildSystemPrompt("");
    // a->render({ChatMessage::from_system(b)});

    // std::cout << "template: " << a->render({ChatMessage::from_system(b)}) << "\n";
    // return 0;

    // DEBUG
    if (argc < 2)
    {
        std::cerr << "Cach dung: " << argv[0] << " <duong-dan-model.gguf>\n";
        return 1;
    }

    std::string model_path = argv[1];

    // Tao engine qua factory function - khong biet implementation ben trong
    std::unique_ptr<Engine> engine(createLlamaEngine());

    std::cout << "Dang load model: " << model_path << " ...\n";

    // n_gpu_layers = 0 -> chay thuan CPU (an toan mac dinh cho test)
    // n_ctx = 2048 -> du de test nhanh
    Result rsl = engine->loadModel(model_path, /*n_gpu_layers=*/0);
    auto ctx = engine->createContextEphemeral();
    auto tmpl = makeChatTemplate(ChatTemplateType::SmolLM3);

    if (!rsl.success)
    {
        std::cerr << rsl.text;
        return 1;
    }

    std::cout << "Load model thanh cong. Go cau hoi (go 'exit' de thoat):\n";

    std::string line;
    std::string prompt = tmpl->buildSystemPrompt("", {}, true);
    std::string full_prompt = tmpl->render({ChatMessage::from_system(prompt)});
    std::cout << "system: " << full_prompt << "\n";

    // std::string full_prompt = tmpl->renderSystem(
    //     "You are a helpful AI assistant. "
    //     "You must call an appropriate tool whenever the user's request needs information "
    //     "or an action you cannot provide from your own knowledge (real-time data, external "
    //     "lookups, file or device actions, etc). Do not guess, fabricate, or apologize instead "
    //     "of calling a tool. "
    //     "The only exception is when the user explicitly asks you to summarize information "
    //     "that is already present in this conversation — in that case, answer directly "
    //     "without calling any tool. "
    //     "Before calling a specific function, you must first call get_tool_detail with the "
    //     "name of the tool you intend to use, to learn its exact functions and parameters. "
    //     "When calling a tool, output only the tool call.",
    //     {// Fake: danh sach tool THAT (mo ta ngan, KHONG phai list_tools nua)
    //      R"({"name": "time", "description": "Get the current date and time."})",
    //      R"({"name": "weather", "description": "Get current weather information for a city."})",
    //      R"({"name": "filesystem", "description": "Read, write, or list files on the local disk."})",
    //      R"({"name": "web_search", "description": "Search the web for up-to-date information."})",

    //      // Meta-tool duy nhat con lai: lay chi tiet ham cua 1 tool cu the
    //      R"({"name": "get_tool_detail", "description": "Get the detailed function signatures and parameters available inside a specific tool.", "parameters": {"type": "object", "properties": {"name": {"type": "string", "description": "The name of the tool to inspect."}}, "required": ["name"]}})"

    //     },
    //     true);

    //////////////////////////////////////////////////
    ctx->acquire(full_prompt);
    ctx->setTurnGrammar();
    //     ctx->setToolCallGrammar(
    //         R"({
    //     "type": "object",
    //     "properties": {
    //         "name": { "enum": ["get_tool_detail"] },
    //         "arguments": {
    //             "type": "object",
    //             "properties": { "name": { "type": "string" } },
    //             "required": ["name"]
    //         }
    //     },
    //     "required": ["name", "arguments"]
    // })",
    //         "<tool_call>");

    while (true)
    {
        std::cout << "\nBan: ";
        if (!std::getline(std::cin, line))
            break;
        if (line == "exit")
            break;
        if (line.empty())
            continue;
        std::string full_prompt = tmpl->render({
                                                   ChatMessage::from_user(line),
                                               },
                                               true, false);
        std::cout << full_prompt;

        rsl = ctx->feedTokens(full_prompt, true);
        if (!rsl.success)
        {
            std::cout << "FAILED TO APPLY TEMPLATE: " << rsl.text << "\n";
            return 1;
        }
        std::cout << "Model: ";
        Result res = ctx->generateStream(
            [](const std::string &piece)
            {
                std::cout << piece;
                std::cout.flush(); // đẩy ra ngay lập tức, không đợi buffer đầy
                return true;
            });
        std::cout << "\n"; // xuống dòng sau khi model in xong (dù thành công hay lỗi)
                           // check xem model có gọi tool không
        if (res.text.find("<tool_call>") != std::string::npos)
        {
            // giả lập tool result (test)
            std::string fake_turn;
            // 1. Đóng lượt assistant trước (EOG chưa được decode)
            fake_turn += "<|im_end|>\n";
            // 2. Mở user turn chứa tool response
            fake_turn += "<|im_start|>user\n";
            fake_turn += "<tool_response>\n";
            fake_turn += R"({"ok": true, "data": {"time": "2026-09-16 14:30"}})";
            fake_turn += "\n</tool_response>";
            fake_turn += "<|im_end|>\n";
            // 3. Mở lượt assistant kế tiếp
            fake_turn += "<|im_start|>assistant\n";

            ctx->feedTokens(fake_turn);

            // generate tiếp lượt 2
            Result res2 = ctx->generateStream([](const std::string &piece)
                                              {
                                                           std::cout << piece;
                                                           std::cout.flush();
                                                           return true; });
            std::cout << "fake tool call  \n";
        }

        if (!res.success)
        {
            std::cerr << "[Loi] " << res.text << "\n";
        }
    }
    ctx->release();

    return 0;
}

/*
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TOOLS=OFF \
    -DLLAMA_BUILD_SERVER=OFF
*/
// cmake --build build -j 5
// ./build/daemon/daemon