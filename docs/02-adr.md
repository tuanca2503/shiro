# Architecture Decision Records — shiro.cpp Agent Runtime

**Quy ước.** Mỗi ADR có số vĩnh viễn. Không xoá, không sửa nội dung. Muốn đổi một quyết định thì viết ADR mới với `Supersedes: ADR-xxx` và đổi `Status` của cái cũ thành `Superseded`.

**Status:** `Accepted` · `Proposed` (chưa ai duyệt) · `Open` (chưa đủ thông tin, xem [04-open-decisions.md](04-open-decisions.md)) · `Rejected` · `Superseded`

| ADR | Tiêu đề | Status |
|---|---|---|
| [001](#adr-001) | Agent Runtime tách khỏi Model | Accepted |
| [002](#adr-002) | Một model instance, tool đổi không reload model | Accepted |
| [003](#adr-003) | Conversation và Tool Registry là hai miền trạng thái độc lập | Accepted |
| [004](#adr-004) | `AgentSession` sở hữu `Conversation` | Accepted (supersedes v1.0) |
| [005](#adr-005) | Output protocol: JSON discriminated union ép bằng GBNF | Accepted |
| [006](#adr-006) | GBNF chỉ ràng buộc format, không chứa orchestration | Accepted |
| [007](#adr-007) | `ToolMode` do `ToolPolicy` sở hữu, quyết định lại mỗi step | Accepted |
| [008](#adr-008) | Tool được expose theo từng step, không theo session | Accepted |
| [009](#adr-009) | Expose tĩnh là mặc định; discovery là cơ chế mở rộng quy mô | Accepted (supersedes v1.0) |
| [010](#adr-010) | Cổng type-token để kiểm soát tool overuse | Proposed |
| [011](#adr-011) | Không dùng router model | Accepted |
| [012](#adr-012) | Tool block đặt cuối prompt để giữ KV cache | Proposed |
| [013](#adr-013) | Tách LLM-facing definition / tool schema / Tool ABI | Accepted |
| [014](#adr-014) | Không gọi tool song song ở MVP | Accepted |
| [015](#adr-015) | Fine-tune là phase cuối, kích hoạt bằng dữ liệu eval | Accepted |
| [016](#adr-016) | Tool nguy hiểm chạy out-of-process | Proposed |
| [017](#adr-017) | Lỗi model-sửa-được trả về model, có trần số lần sửa | Accepted |
| [018](#adr-018) | Không làm verification layer | Accepted |

---

## ADR-001
### Agent Runtime tách khỏi Model

**Status** Accepted · **Nguồn** PLAN 3 §3.1

**Bối cảnh.** PLAN 2 viết pseudo-code trong đó vòng lặp, history, và việc gọi tool trộn lẫn vào một hàm. Dễ dẫn tới thiết kế ngầm `Agent = Model + tools`.

**Quyết định.** `Agent` là một component riêng, sở hữu: vòng lặp, state machine, chính sách tool, mọi kiểm tra an toàn, ngân sách. Model chỉ là engine sinh token, được truy cập qua `ILLMInterface`.

**Phương án đã cân nhắc.** (a) Agent là một hàm tiện ích quanh model — bị loại vì không có chỗ đặt state, cancellation, policy. (b) Agent là một lớp mỏng, logic nằm trong prompt — bị loại vì kiểm tra an toàn không được phép phụ thuộc vào việc model tuân thủ prompt.

**Đánh đổi.** Nhiều component hơn, nhiều code hơn ở giai đoạn đầu.

**Hệ quả.** Mọi quyết định có hệ quả thật nằm ngoài model. `engine/` không được chứa khái niệm tool/agent/session — đây là tiêu chí review code.

---

## ADR-002
### Một model instance, tool đổi không reload model

**Status** Accepted · **Nguồn** [Existing] + PLAN 3 §8

**Bối cảnh.** Có hiểu nhầm phổ biến rằng đổi tool definitions đòi hỏi tạo lại model hoặc context.

**Quyết định.** Engine load đúng một model (SmolLM3), giữ suốt vòng đời process. Tool definitions là một phần của **inference request**, không phải của **model weights**. Đổi `InferenceContext.tools` không chạm tới model.

**Phương án đã cân nhắc.** (a) Nhiều `llama_context`, mỗi session một cái — không loại hẳn, nhưng tốn RAM cho KV cache; xem D10. (b) Reload model khi tool đổi — vô lý về kỹ thuật, loại.

**Đánh đổi.** Một context ⇒ mọi session phải serialize khi decode. Xem §24.2 của design doc.

**Hệ quả.** Ràng buộc "một model" trở thành một tiền đề kiến trúc; bất kỳ đề xuất nào cần model thứ hai (router, verifier) đều phải đối mặt với ADR này.

---

## ADR-003
### Conversation và Tool Registry là hai miền trạng thái độc lập

**Status** Accepted · **Nguồn** PLAN 3 §3.2

**Bối cảnh.** Nếu nhúng danh sách tool vào system message và giữ trong history, mỗi lần đổi tool phải sửa history hoặc reset conversation, và history sẽ tích luỹ nhiều phiên bản catalog mâu thuẫn.

**Quyết định.** `Conversation` chỉ chứa message (system/user/assistant/tool). Tool definitions **không bao giờ** được ghi vào `Conversation`. Chúng được dựng lại vào prompt ở mỗi lần inference từ `ToolSet`.

**Phương án đã cân nhắc.** (a) Tool defs trong system message đầu conversation — loại vì lý do trên. (b) Tool defs ghi lại mỗi lần đổi như một message mới — loại vì history phình và mâu thuẫn.

**Đánh đổi.** Prompt phải dựng lại tool block mỗi lần ⇒ chi phí prefill; xử lý bằng ADR-012.

**Hệ quả.** FR-5 thoả mãn tự nhiên. Kéo theo ADR-004 và ADR-008.

---

## ADR-004
### `AgentSession` sở hữu `Conversation`

**Status** Accepted · **Supersedes** quyết định tương ứng trong bản gộp v1.0 · **Nguồn** PLAN 3 §3.2

**Bối cảnh.** Bản v1.0 cho lớp server giữ session history và truyền read-only vào agent. Cách đó buộc phải tạo ra hai khái niệm song song: "session history" (của server) và "run context" (của agent), rồi phải định nghĩa quy tắc đồng bộ giữa chúng. Sự phân đôi này không mang lại lợi ích nào.

**Quyết định.** `AgentSession { Conversation, ToolSelection, AgentState, AgentConfig }` là đơn vị sở hữu. Server quản lý **vòng đời và lưu trữ** của các session (map id → session, hết hạn, persistence), không quản lý nội dung conversation. `Agent::run(AgentSession&, ...)` ghi trực tiếp vào conversation của session.

**Phương án đã cân nhắc.** (a) Server sở hữu history, agent nhận read-only (v1.0) — loại, lý do trên. (b) Agent sở hữu cả vòng đời session — loại, vì persistence và HTTP là việc của server.

**Đánh đổi.** Agent có quyền ghi vào state của session ⇒ phải bảo đảm đúng một `run` tại một thời điểm trên mỗi session (cờ `busy`, §24.2).

**Hệ quả.** Xoá bỏ khái niệm "run context" như một cấu trúc riêng. Kéo theo yêu cầu thread-safety ở §24.2.

---

## ADR-005
### Output protocol: JSON discriminated union ép bằng GBNF

**Status** Accepted · **Nguồn** PLAN 2 §2.3 (vấn đề) + PLAN 3 §14 (hướng)

**Bối cảnh.** PLAN 2 dùng `find("<tool_call>")` trên text thô: false positive khi model in chuỗi đó trong ví dụ, false negative khi bị cắt token.

**Quyết định.** Model luôn sinh đúng một JSON object thuộc bốn dạng `tool_call | final | no_tool | discover`, trường `type` đứng đầu, ép cứng bằng GBNF. Agent rẽ nhánh theo `type`.

**Phương án đã cân nhắc.** (a) Text tự do + thẻ — loại, lý do trên. (b) Text tự do cho prose + grammar chỉ bật khi nghi ngờ có tool call — loại vì "nghi ngờ" lại cần một cơ chế phát hiện, quay về vấn đề cũ.

**Đánh đổi.** Câu trả lời cuối bị bọc JSON ⇒ phải viết parser tăng tiến để stream nội dung trường `content`. Mất khả năng sinh prose hoàn toàn tự do; bù bằng trường `thought`.

**Hệ quả.** Abstention (`no_tool`) thành công dân hạng nhất. Tạo ra đúng một điểm quyết định để ADR-010 bám vào — đây là hệ quả quan trọng nhất và không lường trước lúc đầu.

---

## ADR-006
### GBNF chỉ ràng buộc format, không chứa orchestration

**Status** Accepted · **Nguồn** PLAN 3 §14, cả ba plan đồng thuận

**Bối cảnh.** Vì GBNF có thể ép output rất chặt, có cám dỗ dùng nó để mã hoá luôn luật nghiệp vụ ("chỉ cho phép tool X sau tool Y").

**Quyết định.** Bốn tầng tách bạch: hạ tầng GBNF (trong `engine/`, không biết tool) → grammar khung của agent (4 nhánh, hằng số) → grammar theo tool (hàm thuần `ToolSet → string`) → parser. Grammar **phản chiếu** quyết định của `ToolPolicy`; nó không đưa ra quyết định.

**Phương án đã cân nhắc.** Mã hoá luật chuyển tiếp tool vào grammar — loại vì luật sẽ nằm ở nơi không test được, không log được, không giải thích được cho người dùng khi bị chặn.

**Đánh đổi.** Không.

**Hệ quả.** `ToolMode::None` biểu hiện thành "grammar không có nhánh tool_call" — grammar là *hệ quả*, policy là *nguyên nhân*. Tiêu chí review: grep tên tool trong `engine/` phải ra rỗng.

---

## ADR-007
### `ToolMode` do `ToolPolicy` sở hữu, quyết định lại mỗi step

**Status** Accepted · **Nguồn** PLAN 3 §10, mở rộng

**Bối cảnh.** PLAN 3 định nghĩa `ToolMode` nhưng không nói ai đặt nó và đặt khi nào.

**Quyết định.** `ToolPolicy` (trong runtime) quyết định `ToolMode` và tập tool expose, **ở mỗi step**, dựa trên: `AgentState`, `ToolSelection` của session, cấu hình, và kích thước catalog. Model không bao giờ được đề nghị đổi mode. Thêm mode thứ năm `Discovery` ngoài bốn mode của PLAN 3.

**Phương án đã cân nhắc.** (a) `ToolMode` cố định theo session — loại vì discovery và chuỗi nhiều bước cần đổi mode giữa chừng. (b) Model tự chọn mode — loại, vi phạm ADR-001.

**Đánh đổi.** Thêm một component. Nhưng nó là nơi duy nhất hợp lý để đặt logic của ADR-009 và ADR-010.

**Hệ quả.** `ToolMode` xuất hiện trong log operational mỗi step; nếu không log, không debug được vì sao model không gọi tool.

---

## ADR-008
### Tool được expose theo từng step, không theo session

**Status** Accepted · **Nguồn** PLAN 3 §7 + [Derived]

**Quyết định.** Mỗi step, `ToolRegistry::snapshot(selection)` trả về một `ToolSet` **bất biến**; `ContextBuilder` và `GrammarBuilder` cùng nhận đúng snapshot đó với cùng `registry_epoch`.

**Phương án đã cân nhắc.** Dựng tool block một lần mỗi turn — loại, vì discovery mở rộng tập tool ngay giữa turn.

**Đánh đổi.** Một lần copy con trỏ mỗi step, không đáng kể.

**Hệ quả.** Một thay đổi registry đến giữa bước chỉ có hiệu lực từ bước sau. Bảo đảm bất biến quan trọng nhất của hệ thống: **prompt và grammar không bao giờ lệch nhau**. Thiếu nó, model có thể bị ép sinh lời gọi tới hàm mà nó chưa hề thấy mô tả.

---

## ADR-009
### Expose tĩnh là mặc định; discovery là cơ chế mở rộng quy mô

**Status** Accepted · **Supersedes** quyết định tương ứng trong bản gộp v1.0 · **Nguồn** PLAN 3 §13

**Bối cảnh.** Bản v1.0 coi flow discovery hai tầng (`list_tools` → `get_tool_detail`) là luồng mặc định. PLAN 3 chỉ ra điều mà v1.0 bỏ sót: **`list_tools` bản thân cũng là một tool**, nên với model nhỏ có xu hướng gọi tool bừa, discovery không chữa được overuse mà còn làm nặng thêm — model sẽ gọi `list_tools()` cho `hello`.

Thêm vào đó, discovery cộng 2 step vào mỗi tác vụ, làm lỗi cộng dồn (P7) tệ hơn và độ trễ tăng 2–3 lần.

**Quyết định.** Mặc định là expose tĩnh tập tool đã chọn cho turn. `ToolMode::Discovery` chỉ bật khi số tool expose vượt `static_threshold` (đề xuất khởi điểm 8 tool / 800 token). Discovery vẫn được hiện thực và test, nhưng không nằm trên đường mặc định. Với MVP (≤8 tool) discovery **không chạy**.

**Phương án đã cân nhắc.** (a) Discovery luôn bật (v1.0) — loại, lý do trên. (b) Không bao giờ làm discovery — loại vì catalog sẽ lớn khi plugin tăng, và prompt phình sẽ đánh vào NFR-1.

**Đánh đổi.** Prompt to hơn khi catalog nhỏ. Chấp nhận được: 8 tool × ~100 token = 800 token, rẻ hơn nhiều so với 2 round trip.

**Hệ quả.** Discovery chuyển từ "kiến trúc cốt lõi" sang "tính năng có ngưỡng kích hoạt". Ngưỡng phải đo, không đoán → D4.

---

## ADR-010
### Cổng type-token để kiểm soát tool overuse

**Status** **Proposed** — cần D1 và dữ liệu eval · **Nguồn** [Proposal] của tài liệu này

**Bối cảnh.** Vấn đề P1: SmolLM3 gọi tool cho `hello`. PLAN 3 nhận diện đúng nhưng chỉ đề xuất ToolMode, mà ở chế độ `Auto` thì vẫn là model tự quyết. Thêm câu "only call tools when necessary" vào prompt không phải giải pháp kiến trúc.

**Quyết định.** Vì ADR-005 ép trường `type` đứng đầu JSON, quyết định "gọi tool hay trả lời thẳng" thu về đúng một vị trí token, nơi grammar đã mask sạch token không hợp lệ. Runtime đọc xác suất tại vị trí đó; nếu `P(tool_call) < threshold` thì mask `tool_call`, ép model đi nhánh `final`. Ngưỡng cấu hình theo `ToolMode`, tinh chỉnh bằng eval set.

**Vì sao hợp lệ, trong khi PLAN 1 (P2) nói xác suất token không đáng tin.** PLAN 1 đúng trong ngữ cảnh của nó: xác suất token không phải ước lượng đáng tin cho *tính đúng sự thật* của một câu trả lời. Ở đây câu hỏi khác hẳn: một **phân loại nhị phân rời rạc**, và xác suất tại token đó **chính là** độ tự tin của model về quyết định đó, không qua trung gian. Hai bài toán khác nhau; lập luận calibration của PLAN 1 không áp dụng. Điều này **không** có nghĩa cổng luôn đúng — chỉ có nghĩa ngưỡng là thứ tinh chỉnh được bằng dữ liệu.

**Phương án đã cân nhắc.**
- (a) Chỉ dựa prompt (L1) — không đủ, PLAN 3 đã nói.
- (b) Heuristic từ khoá (PLAN 3 Level 2) — rẻ nhưng hẹp và khó mở rộng; giữ làm fast-path tuỳ chọn.
- (c) Pass phân loại riêng bằng chính model đó (L4) — tốt hơn nhưng tốn thêm một lần sinh; giữ làm dự phòng nếu D1 kết luận không can thiệp được sampler.
- (d) Router model (Level 3) — xem ADR-011.

**Đánh đổi.** Cần engine expose hook tại vị trí sampling → D1. Nguy cơ **chặn nhầm**: câu thực sự cần tool bị ép trả lời từ trí nhớ tham số — âm thầm và nguy hiểm hơn overuse.

**Hệ quả.** Ba điều kiện bắt buộc trước khi bật cổng ở môi trường thật: (1) eval set có cả hai lớp câu hỏi, (2) đường cong precision/recall để chọn điểm làm việc, (3) metric hai chiều `gate_blocked` và tỉ lệ chặn nhầm. Không có ba thứ này thì cổng chỉ là mê tín.

---

## ADR-011
### Không dùng router model

**Status** Accepted · **Nguồn** PLAN 3 §12 Level 3 (cân nhắc rồi loại)

**Quyết định.** Không đưa model thứ hai vào kiến trúc, kể cả một model phân loại rất nhỏ.

**Lý do, theo thứ tự sức nặng.** (1) Mâu thuẫn ràng buộc đã chốt: engine chỉ load và dùng một model. Đổi nó là đổi một tiền đề kiến trúc, cần bằng chứng rất mạnh. (2) Trùng chức năng với ADR-010, vốn miễn phí. (3) Chi phí thật: RAM cho model thứ hai, một lần inference nữa, một bề mặt cần eval và versioning riêng.

**Phương án đã cân nhắc.** Router model nhỏ phân loại `direct | time | web | fs | db` (PLAN 3 Level 3) — ghi nhận là mở rộng tương lai, chỉ xem xét lại khi ADR-010 **và** phương án L4 đều đo ra không đủ.

**Đánh đổi.** Nếu SmolLM3 hoá ra không thể phân loại tốt ngay cả với cổng và với fine-tune, ta sẽ phải quay lại quyết định này muộn hơn mong muốn.

**Hệ quả.** Mọi đề xuất "thêm một model nhỏ để…" phải mở một ADR mới supersede ADR này.

---

## ADR-012
### Tool block đặt cuối prompt để giữ KV cache

**Status** **Proposed** — cần D3 (A/B eval) · **Nguồn** [Proposal]; cả ba plan đều bỏ sót

**Bối cảnh.** Tool definitions đổi theo từng step (ADR-008). Chat template thông thường đặt chúng trong system message, tức trước conversation history. Trên llama.cpp, mỗi lần tool block đổi sẽ vô hiệu hoá KV cache của toàn bộ phần sau nó — tức toàn bộ history — và phải prefill lại. Với SmolLM3 trên CPU và prompt vài nghìn token, cái giá này là hàng trăm ms **mỗi step**, nhân với số step trong vòng lặp. Đây là yếu tố ảnh hưởng độ trễ lớn nhất của hệ thống.

**Quyết định.** Bố cục prompt xếp theo độ ổn định giảm dần: `[A] system instructions` → `[B] conversation history` (chỉ thêm cuối) → `[C] tool block` (dựng lại mỗi step) → `[D] điểm sinh`. Prefix cache của `A+B` sống sót qua mọi thay đổi tool; chỉ `C+D` bị re-prefill.

Điều này khả thi vì `shiro.cpp` fix cứng chat template theo model, nên nó **có** quyền đặt tool block ở vị trí muộn (ví dụ một message vai `system` chèn ngay trước lượt user cuối).

**Phương án đã cân nhắc.** (a) Tool block trong system message theo chuẩn — loại vì chi phí prefill. (b) Giữ tool block cố định cả session để cache không đổi — loại vì mâu thuẫn ADR-008 và FR-5.

**Đánh đổi.** Chưa biết SmolLM3 tuân thủ tool definitions ở vị trí muộn tốt đến đâu. Đây là câu hỏi thực nghiệm, không phải câu hỏi thiết kế → **D3**, giải bằng A/B eval so sánh tỉ lệ chọn đúng tool giữa hai bố cục.

**Hệ quả.** Cache chỉ bị vô hiệu hoá khi nén history (§19 của design doc), việc hiếm. Cần metric "tỉ lệ KV cache hit trên prefix" để xác nhận quyết định này thực sự có tác dụng.

---

## ADR-013
### Tách LLM-facing definition / tool schema / Tool ABI

**Status** Accepted · **Nguồn** PLAN 3 §15 + [Derived]

**Quyết định.** Bốn khái niệm riêng biệt, không trộn: **LLM-facing definition** (tập con vào prompt + grammar) ⊂ **ToolDefinition** (bản ghi đầy đủ trong registry) · **Tool ABI** (`ITool::execute`, hợp đồng C++ độc lập với model) · **plugin loading** (cách nạp — D2).

**Phương án đã cân nhắc.** Một struct duy nhất dùng chung cho cả prompt và thực thi — loại, vì sẽ vô tình đẩy `timeout_ms`, `capabilities`, đường dẫn hiện thực vào prompt, làm lộ bề mặt nội bộ và tốn token vô ích.

**Đánh đổi.** Phải viết một hàm chiếu `ToolDefinition → LLM-facing`, và phải nhớ cập nhật nó khi thêm trường.

**Hệ quả.** Nếu plugin biên dịch thẳng vào binary thì **không có vấn đề ABI** và tầng đó thu về một interface C++ bình thường. Nếu nạp `.so` động thì cần chữ ký export, layout struct ổn định, quy ước version — và mọi thay đổi `ITool`/`ToolResult` thành breaking change.

---

## ADR-014
### Không gọi tool song song ở MVP

**Status** Accepted · **Nguồn** [Derived]

**Quyết định.** Một tool mỗi step. Tuần tự.

**Lý do.** Thêm concurrency vào loop; ngữ nghĩa lỗi một phần phức tạp; model nhỏ hiếm khi tận dụng đúng; vi phạm nguyên tắc "đơn giản nhất thoả yêu cầu".

**Đánh đổi.** Chậm hơn với những tác vụ có các lời gọi độc lập.

**Hệ quả.** Kiến trúc để mở: grammar đổi `tool_call` → mảng, `ToolRuntime` chạy song song, **state machine không cần đổi**. Xem lại sau khi có metrics cho thấy độ trễ bị chi phối bởi số step.

---

## ADR-015
### Fine-tune là phase cuối, kích hoạt bằng dữ liệu eval

**Status** Accepted · **Nguồn** PLAN 1 (đảo thứ tự)

**Bối cảnh.** PLAN 1 đặt distillation + LoRA ở Phase 3–4, tức là fine-tune **trước khi** có bất kỳ số đo nào chứng minh model có sẵn không đủ. Đó là tối ưu hoá mù.

**Quyết định.** Fine-tune nằm ở phase cuối cùng và **chỉ chạy nếu** eval cho thấy model có sẵn không đạt ngưỡng, và các biện pháp runtime (mô tả tool, cổng type-token, ToolMode) đã cạn.

**Phương án đã cân nhắc.** Fine-tune sớm để "có model riêng" — loại. Nếu model có sẵn đạt ngưỡng, toàn bộ pipeline distillation + LoRA là công sức bỏ đi.

**Đánh đổi.** Nếu cuối cùng vẫn phải fine-tune thì đã mất thời gian chờ.

**Hệ quả.** Lợi ích lớn nhất của việc đảo thứ tự: **nguồn dữ liệu huấn luyện tốt nhất là log thật từ phase eval**, không phải tình huống bịa ra bằng teacher model. Và bộ eval đã có sẵn làm thước đo, không phải xây mới. Nếu chạy, mục tiêu fine-tune cụ thể nhất là chính quyết định ở ADR-010.

---

## ADR-016
### Tool nguy hiểm chạy out-of-process

**Status** **Proposed** — phụ thuộc D2 · **Nguồn** PLAN 2 §3.3 + [Derived]

**Bối cảnh.** Tool hiện thực bằng lời gọi blocking không phản ứng với cancellation token. Không được `pthread_cancel` hay kill thread trong C++ — hỏng state, rò tài nguyên.

**Quyết định.** Phương án lai: tool `safe` và `sensitive` chạy in-process (nhanh, đơn giản); tool `dangerous` hoặc có capability `os:exec` chạy trong tiến trình con.

**Phương án đã cân nhắc.** (a) Tất cả in-process — loại vì không kill được và không sandbox được. (b) Tất cả out-of-process — loại vì IPC cho mọi lời gọi tool là chi phí vô ích với tool đọc giờ.

**Đánh đổi.** Hai đường thực thi phải bảo trì; cần định nghĩa IPC.

**Hệ quả.** Với tool in-process bị timeout: **bỏ rơi** thread, agent đi tiếp với `ToolResult{timeout}`, kết quả về muộn bị vứt nhờ kiểm tra epoch. Đây là hành vi được thiết kế có chủ ý, không phải rò rỉ.

---

## ADR-017
### Lỗi model-sửa-được trả về model, có trần số lần sửa

**Status** Accepted · **Nguồn** PLAN 1 P4/ADR-006 + PLAN 2 §4 (hợp nhất)

**Bối cảnh.** PLAN 1 chia lỗi làm hai loại (model-level / runtime-level); PLAN 2 liệt kê 5+ loại rời rạc. Hai cách phân loại không tương thích.

**Quyết định.** Một taxonomy duy nhất, mỗi lỗi có bốn thuộc tính: *phát sinh ở đâu · trả lại model hay không · có tiếp tục không · người dùng thấy gì*. Nguyên tắc chia: **lỗi model sửa được thì đưa lại cho model; lỗi model không sửa được thì dừng.** Mỗi lớp lỗi có trần `max_repair` (đề xuất 2).

**Phương án đã cân nhắc.** Trả mọi lỗi về model để nó tự xoay xở — loại: đưa lỗi OOM cho model là vô nghĩa và đốt ngân sách.

**Đánh đổi.** Phải duy trì một bảng, và mỗi tool mới phải khai `error_codes`.

**Hệ quả.** Thông điệp lỗi phải hướng dẫn được cách sửa (`invalid_args: trường 'city' bắt buộc, kiểu string; nhận được null`), không phải chỉ `error`. Không có trần thì model có thể đốt sạch budget cho `parse_error`.

---

## ADR-018
### Không làm verification layer

**Status** Accepted · **Nguồn** PLAN 1 ADR-004 (loại)

**Bối cảnh.** PLAN 1 đề xuất một tầng verification riêng với ba phương án: self-consistency, bắt buộc ground bằng retrieval, hoặc một verifier model riêng.

**Quyết định.** Không làm ở kiến trúc này.

**Lý do.** (1) Self-consistency nhân số lần sinh, mâu thuẫn trực tiếp mục tiêu độ trễ. (2) Verifier model riêng mâu thuẫn ADR-011 và ADR-002. (3) "Bắt buộc ground bằng retrieval" thực chất đã được thực hiện ở dạng khác: `no_tool` là output hợp lệ và kết quả tool là nguồn sự thật duy nhất cho dữ kiện. (4) Rủi ro thật mà verification nhắm tới — hành động sai gây hậu quả — được xử lý rẻ hơn và chắc hơn bằng xác nhận người dùng cho hành động không hoàn tác.

**Phương án đã cân nhắc.** Bật verification chỉ cho tool rủi ro cao — nhưng với tool rủi ro cao thì xác nhận người dùng đã là lớp mạnh hơn, và verification chỉ thêm độ trễ.

**Đánh đổi.** Mất một lớp bắt sai cho câu trả lời thuần văn bản.

**Hệ quả.** Chất lượng câu trả lời được bảo đảm bằng eval hồi quy (đo và lặp), không bằng một component runtime. Xem lại nếu dữ liệu eval cho thấy một lớp sai cụ thể mà eval không bắt được. Xem D7.