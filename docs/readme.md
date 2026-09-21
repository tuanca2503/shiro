# shiro.cpp — Agent Runtime · Bộ tài liệu thiết kế

| | |
|---|---|
| **Hệ thống** | `shiro.cpp` — Agent Runtime chạy trên engine wrap llama.cpp |
| **Model** | SmolLM3 (một instance duy nhất) |
| **Trạng thái** | `Draft — In Review` · chưa có code agent |
| **Version** | 2.0 |
| **Thay thế** | PLAN 1 (*Task-Routing Orchestrator*), PLAN 2 (*Đánh giá kiến trúc Agent C++*), PLAN 3 (*Agent Runtime Multi-Context*), và bản gộp v1.0 |
| **Owner** | *Chưa xác định* |
| **Reviewer** | *Chưa xác định* |

---

## Cách đọc bộ tài liệu này

Bốn tài liệu, mỗi tài liệu một mục đích. Không có nội dung nào lặp lại giữa chúng — nếu cần tham chiếu thì dùng liên kết, không copy.

| File | Nội dung | Đọc khi nào |
|---|---|---|
| [`01-design.md`](01-design.md) | **Technical Design Document.** Kiến trúc, trách nhiệm component, interface, lifecycle, state machine, protocol, bảo mật, hiệu năng. Đây là tài liệu tham chiếu chính. | Trước khi viết bất kỳ dòng code nào; khi tranh luận về ranh giới component |
| [`02-adr.md`](02-adr.md) | **Architecture Decision Records.** 18 quyết định kiến trúc, mỗi cái có bối cảnh, phương án đã cân nhắc, đánh đổi, hệ quả. Bản ghi bất biến — sửa bằng cách thêm ADR mới supersede ADR cũ. | Khi hỏi "vì sao lại làm thế này?"; khi muốn đổi một quyết định |
| [`03-implementation-plan.md`](03-implementation-plan.md) | **Kế hoạch triển khai.** Phase, exit criteria, chiến lược test, acceptance criteria, ma trận truy vết. | Khi lập kế hoạch sprint; khi nghiệm thu một phase |
| [`04-open-decisions.md`](04-open-decisions.md) | **Sổ quyết định còn treo.** D1–D14, mỗi cái nêu rõ chặn phase nào, cần thông tin gì để chốt. | Hàng tuần; trước khi bắt đầu mỗi phase |

---

## Tóm tắt một trang

**Vấn đề trung tâm:** SmolLM3 có xu hướng gọi tool cả khi không cần (`hello` → `tool_call`). Ba plan gốc không plan nào giải quyết triệt để: PLAN 1 và PLAN 2 không nhắc tới; PLAN 3 nhận diện đúng nhưng chỉ đề xuất ToolMode mà không nói ai quyết định mode.

**Hướng giải quyết:** kiểm soát theo 5 lớp, rẻ trước đắt sau (§12 của `01-design.md`), trong đó lớp then chốt là **cổng type-token**: vì output bị GBNF ép thành JSON có trường phân loại `"type"` ở đầu, quyết định *gọi tool hay trả lời thẳng* thu về đúng **một token**. Runtime đọc phân bố xác suất tại token đó và áp ngưỡng — được một router chạy trong runtime, không cần model thứ hai, không tốn thêm một lần inference nào.

**Ba thay đổi lớn so với bản v1.0:**

1. **Discovery không còn là luồng mặc định.** PLAN 3 nói đúng: `list_tools` bản thân nó cũng là một tool, nên với model nhỏ, discovery *làm nặng thêm* vấn đề gọi tool thừa chứ không chữa được. Mặc định giờ là expose tool tĩnh theo từng turn; discovery chỉ kích hoạt khi catalog vượt ngưỡng (ADR-009).
2. **`AgentSession` sở hữu `Conversation`**, server chỉ quản lý vòng đời session. Bản v1.0 để server giữ history và truyền read-only vào agent — cách đó tạo ra sự phân đôi thừa giữa "session history" và "run context" (ADR-004).
3. **Tool definitions đặt cuối prompt, không đặt trong system message.** Lý do là KV cache: tool block thay đổi theo từng turn, nếu nó nằm trước conversation history thì mọi thay đổi tool sẽ vô hiệu hoá toàn bộ cache của history và phải prefill lại từ đầu (ADR-012).

**Phạm vi MVP:** một model, tool expose tĩnh ≤8 tool, protocol JSON, tool tuần tự, cancellation, taxonomy lỗi, tool in-process an toàn. **Không** có: discovery, router model, tool nguy hiểm, compaction, fine-tune.

**Chặn ngay:** D1 (hook sampler cho cổng type-token), D2 (mô hình nạp plugin), D3 (vị trí tool block trong prompt). Xem `04-open-decisions.md`.

---

## Quy ước

- **[Existing]** — đã có trong plan gốc hoặc là quyết định đã chốt trước đó.
- **[Derived]** — suy ra logic từ tài liệu nguồn.
- **[Proposal]** — đề xuất của tài liệu này, chưa được xác nhận.
- **[Decision Required]** — thiếu thông tin, xem `04-open-decisions.md`.
- ADR được đánh số vĩnh viễn. Không xoá, không sửa nội dung — chỉ đổi `Status` và thêm ADR mới `Supersedes`.

## Changelog

| Version | Ngày | Thay đổi |
|---|---|---|
| 1.0 | — | Gộp PLAN 1 + PLAN 2 |
| 2.0 | 2026-09-17 | Gộp thêm PLAN 3; tách thành bộ 4 tài liệu; đảo mặc định discovery (ADR-009); đổi chủ sở hữu conversation (ADR-004); thêm ToolMode và cổng type-token (ADR-007, ADR-010); thêm bố cục prompt theo KV cache (ADR-012); thêm mục đồng thời/thread-safety |