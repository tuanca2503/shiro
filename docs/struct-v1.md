shiro/
├── vendor/                 # Submodule, KHÔNG sửa, chỉ pull update
├── engine/                 # bọc quanh llama.cpp
│   ├── include/
│   │   └── engine.h        # API công khai của engine (interface ổn định)
│   ├── src/
│   │   └── engine.cpp      # implementation, gọi vào llama.cpp bên dưới
│   └── CMakeLists.txt
├── agent/                  # thư viện của bạn, KHÔNG biết gì về llama.cpp
│   ├── include/
│   │   └── agent.h         # chỉ biết gọi engine qua interface của engine.h
│   ├── src/
│   │   ├── loop.cpp        # agent loop (nhận input → hỏi engine → xử lý tool)
│   │   ├── tool_call.cpp   # logic gọi tool
│   │   └── mcp.cpp         # xử lý MCP protocol
│   └── CMakeLists.txt
├── models/                 # nơi chứa weight — KHÔNG commit vào git (rất nặng)
│   ├── base/               # model pull nguyên bản từ Hugging Face (SmolLM3-3B gốc)
│   └── finetuned/          # model sau khi bạn tự finetune, có version để dễ so 
├── datasets/               # dữ liệu finetune sau này
├── daemon/                 # executable chính, KHÔNG chứa logic nghiệp vụ
│   ├── main.cpp            # chỉ khởi tạo engine + agent, wire chúng lại
│   ├── http_server.cpp     # public API (dùng cpp-httplib)
│   └── CMakeLists.txt
└── .gitignore