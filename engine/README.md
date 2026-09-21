

n_ctx_ = 4096 # max context
n_max_seq_ = 4  # max pool

kv_unified = true # dùng một KV buffer chung cho tất cả sequence; còn khi tắt unified thì n_ctx mới bị chia theo n_seq_max

ContextLifecycle {
    Persistent # Muốn A chạy lại thì phải có sequence được release theo lifecycle của conversation.  

    Ephemeral # Vì Ephemeral vốn đã trả sequence về ngay sau request.

}


system_prompt_ = "...";
max_tokens_ = 256;

        int n_past_ = 0;
                int n_keep_ = 0;









                Context ContextManager::create(
    uint32_t n_ctx,
    uint32_t n_seq_max,
    bool kv_unified,
    ContextLifecycle lifecycle)
{
    llama_context_params params = llama_context_default_params();

    params.n_ctx = n_ctx;
    params.n_seq_max = n_seq_max;
    params.n_threads = n_threads_;
    params.kv_unified = kv_unified;

    llama_context* ctx = llama_init_from_model(model_, params);

    if (!ctx) {
        // xử lý lỗi
    }

    const uint32_t id = static_cast<uint32_t>(contexts_.size());
    contexts_.push_back(ctx);

    Context result;
    result.manager_ = this;
    result.context_id_ = id;
    result.n_ctx_ = n_ctx;
    result.n_seq_max_ = n_seq_max;
    result.kv_unified_ = kv_unified;
    result.lifecycle_ = lifecycle;

    return result;
}

Khi đó Agent dùng đúng như bạn muốn:

auto classifier = engine.contexts().create(
    4096,
    4,
    true,
    ContextLifecycle::Ephemeral);

auto conversation = engine.contexts().create(
    32768,
    4,
    true,
    ContextLifecycle::Persistent);

Và:

classifier
    = Context object
    ├── config
    └── context_id → ContextManager

conversation
    = Context object
    ├── config
    └── context_id → ContextManager

Còn:

ContextManager
    ├── llama_context #0
    ├── llama_context #1
    └── llama_context #2

Điểm quan trọng là Agent không cầm llama_context* và cũng không cần biết model_.

Tên class trả về tôi thấy đơn giản nhất là Context, vì Agent đang cầm:

auto classifier = ...

và classifier thực sự đại diện cho một context mà Agent đã tạo, chứ không chỉ là một ID.