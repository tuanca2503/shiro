shiro/
├── engine/                 # Tầng 1: load model, serve HTTP/TCP, quản lý request
│   ├── server.py
│   └── model_loader.py
├── agent/                  # Tầng 2: vòng lặp agent — quyết định gọi tool, tổng hợp
│   ├── loop.py
│   └── tools/
│       ├── get_time.py
│       └── web_search.py
├── models/                 # nơi chứa weight — KHÔNG commit vào git (rất nặng)
│   ├── base/               # model pull nguyên bản từ Hugging Face (SmolLM3-3B gốc)
│   └── finetuned/          # model sau khi bạn tự finetune, có version để dễ so 
├── datasets/               # dữ liệu finetune sau này
└── .gitignore