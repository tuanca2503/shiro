// engine/src/engine.cpp — CHỈ file này (và các file trong engine/) được đụng tới llama.h
#include <llama.h>          // ưu tiên header từ include paths / dependency
#include "engine.h"         // interface của bạn

class LlamaEngine : public Engine {
    llama_model* model_;
    // implementation gọi vào llama_decode(), llama_tokenize()...
};