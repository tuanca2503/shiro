# Sổ quyết định còn treo — shiro.cpp Agent Runtime

**Version** 2.0 · Rà soát hàng tuần và trước khi bắt đầu mỗi phase.
Quyết định đã chốt nằm ở [02-adr.md](02-adr.md). Khi một mục ở đây được chốt, viết ADR tương ứng rồi đổi `Status` ở đây thành `Closed → ADR-xxx`.

---

## Bảng tổng

| ID | Quyết định | Chặn | Mức | Status |
|---|---|---|---|---|
| [D1](#d1) | Engine có expose được hook can thiệp sampler tại token quyết định? | Phase 5 | **P0** | Open |
| [D2](#d2) | Plugin nạp thế nào: biên dịch thẳng / `.so` / tiến trình con / lai | Phase 3, 7 | **P0** | Open |
| [D3](#d3) | Tool block đặt đâu trong prompt | Phase 2, 8 | **P0** | Open |
| [D4](#d4) | Ngưỡng chuyển sang discovery | Phase 6b | P1 | Open |
| [D5](#d5) | Kênh xác nhận người dùng qua HTTP | Phase 7 | P1 | Open |
| [D6](#d6) | Session có bền vững qua restart daemon không | Phase 8 | P1 | Open |
| [D7](#d7) | Có bao giờ làm verification layer | sau Phase 8 | P2 | Open |
| [D8](#d8) | Có fine-tune không, base model nào | Phase 10 | P2 | Open |
| [D9](#d9) | Con số mục tiêu: latency, accuracy, tỉ lệ gọi tool thừa | Phase 5, 8 | **P0** | Open |
| [D10](#d10) | Số run đồng thời; một `llama_context` hay nhiều | Phase 8 | P1 | Open |
| [D11](#d11) | Cơ chế lưu secret cho tool | Phase 7 | P1 | Open |
| [D12](#d12) | Xử lý KV cache khi huỷ giữa chừng | Phase 2 | P1 | Open |
| [D13](#d13) | Một plugin nhiều function — định danh và không gian tên | Phase 3 | P2 | Open |
| [D14](#d14) | `ToolSelection` mặc định cho một session mới | Phase 5 | P2 | Open |

---

## D1
### Engine có expose được hook can thiệp sampler tại token quyết định?

**Chặn** Phase 5 · **Mức** P0 · **Liên quan** [ADR-010](02-adr.md#adr-010)

**Vì sao quan trọng.** Cổng type-token — cơ chế trung tâm giải vấn đề tool overuse — đòi hỏi runtime đọc được phân bố xác suất tại đúng vị trí token phân loại, sau khi grammar đã mask, và can thiệp được vào đó. llama.cpp cho phép ở tầng sampler chain, nhưng lớp wrap hiện tại của `shiro.cpp` chưa expose.

**Cần biết.** (a) Lớp wrap có chỗ chèn một sampler tuỳ biến hoặc một callback trước khi chọn token không? (b) Có xác định được *vị trí* nào là token quyết định không — cách đơn giản là đếm: grammar cố định nên vị trí đó là hằng số so với đầu output. (c) Chi phí thực tế bao nhiêu.

**Phương án.**
1. Expose hook — rẻ nhất khi chạy, cần sửa engine một ít.
2. Không hook, dùng phương án L4: một pass phân loại riêng bằng chính model đó, grammar ép `{"needs_tool":true|false}`, sinh ~5 token. Không cần sửa engine, nhưng tốn thêm một lần prefill + decode mỗi turn.
3. Không làm gì, chỉ dựa `ToolMode` do runtime đặt sẵn — chỉ đủ cho các luồng có kịch bản, không đủ cho `Auto`.

**Cách chốt.** Đọc code sampler của engine trong một buổi. Nếu hook khả thi trong dưới nửa ngày công thì chọn (1).

---

## D2
### Plugin nạp thế nào

**Chặn** Phase 3 (định nghĩa Tool ABI) và Phase 7 (sandbox, kill) · **Mức** P0 · **Liên quan** [ADR-013](02-adr.md#adr-013), [ADR-016](02-adr.md#adr-016)

**Vì sao quan trọng.** Quyết định này lan ra ba chỗ: có tồn tại vấn đề ABI hay không, tool nguy hiểm sandbox được hay không, và tool treo có kill được hay không.

**Phương án.**

| | Biên dịch thẳng | `.so` động | Tiến trình con | **Lai (đề xuất)** |
|---|---|---|---|---|
| Vấn đề ABI | Không | Có, phải quản version | Chỉ là IPC schema | Chỉ cho nhánh out-of-process |
| Thêm tool mới | Build lại binary | Thả file `.so` | Thả binary | Tuỳ loại |
| Sandbox được | Không | Không | Có | Có cho tool nguy hiểm |
| Kill khi treo | Không | Không | Có | Có cho tool nguy hiểm |
| Chi phí mỗi lời gọi | 0 | 0 | IPC | 0 cho tool an toàn |
| Độ phức tạp | Thấp nhất | Trung bình | Cao | Cao nhất |

**Đề xuất.** Lai: tool `safe`/`sensitive` biên dịch thẳng (đơn giản, nhanh); tool `dangerous`/`os:exec` chạy tiến trình con. `.so` động chỉ cần khi muốn thả plugin mà không build lại — chưa có yêu cầu đó.

**Cách chốt.** Trả lời một câu: **ai sẽ viết plugin — chỉ nhóm phát triển, hay cả bên thứ ba?** Nếu chỉ nhóm phát triển thì `.so` động là phức tạp thừa.

---

## D3
### Tool block đặt đâu trong prompt

**Chặn** Phase 2 (ảnh hưởng chiến lược cache) và Phase 8 (A/B eval) · **Mức** P0 · **Liên quan** [ADR-012](02-adr.md#adr-012)

**Vì sao quan trọng.** Tool block đổi mỗi step. Nếu nó nằm trước conversation history thì mọi thay đổi tool vô hiệu hoá KV cache của toàn bộ history và phải prefill lại — hàng trăm ms mỗi step trên CPU, nhân với số step. Đây là yếu tố ảnh hưởng độ trễ lớn nhất.

**Cần biết.** SmolLM3 tuân thủ tool definitions ở vị trí muộn (ngay trước lượt user cuối) tốt đến đâu so với đặt trong system message?

**Cách chốt.** A/B eval trên lớp S và lớp M của bộ eval, hai bố cục, cùng mọi thứ khác. Đo: per-step tool accuracy, tỉ lệ tham số đúng, và tỉ lệ KV cache hit. Đây là câu hỏi thực nghiệm, không tranh luận được.

**Dự phòng.** Nếu SmolLM3 tuân thủ kém ở vị trí muộn: giữ tool block trong system message nhưng **đóng băng nó theo turn** thay vì theo step (chỉ dựng lại khi `ToolSelection` hoặc `registry_epoch` đổi), chấp nhận mất tính linh hoạt trong-turn của discovery để đổi lấy cache.

---

## D4
### Ngưỡng chuyển sang discovery

**Chặn** Phase 6b · **Mức** P1 · **Liên quan** [ADR-009](02-adr.md#adr-009)

**Đề xuất khởi điểm** 8 tool hoặc 800 token catalog.

**Cách chốt.** Vẽ hai đường theo số tool: (a) chi phí prefill của tool block ở chế độ tĩnh, (b) chi phí 2 round trip discovery. Giao điểm là ngưỡng. Cần số đo từ Phase 8.

**Ghi chú.** Ngưỡng này không cần chính xác. Sai 2–3 tool không gây hại; chọn một con số, ghi lại lý do, hiệu chỉnh sau.

---

## D5
### Kênh xác nhận người dùng qua HTTP

**Chặn** Phase 7 · **Mức** P1

**Vì sao quan trọng.** Với CLI, xác nhận là một prompt stdin. Với HTTP, agent đang ở giữa một vòng lặp và cần hỏi người dùng rồi chờ — cần một kênh hai chiều. Không có nó, tool `dangerous` không dùng được qua server.

**Phương án.** (a) SSE gửi sự kiện `confirm_required` + một endpoint `POST /confirm`, agent chờ với timeout. (b) WebSocket hai chiều. (c) Trả về ngay một `AgentOutcome{status: needs_confirmation}` và để client gọi lại `resume(run_id, decision)` — không cần giữ kết nối, nhưng cần lưu trạng thái run dở.

**Ghi chú.** (c) hợp với mô hình daemon systemd hơn cả và tránh phải giữ thread chờ, nhưng kéo theo yêu cầu lưu trạng thái run — liên quan D6.

---

## D6
### Session có bền vững qua restart daemon không

**Chặn** Phase 8 · **Mức** P1 · **Liên quan** [ADR-004](02-adr.md#adr-004)

**Vì sao quan trọng.** Server chạy daemon qua systemd với tự restart. Nếu session chỉ nằm trong RAM thì mọi hội thoại mất khi restart. Nếu bền vững thì cần định dạng lưu, version cho định dạng đó, và chiến lược hết hạn.

**Cần quyết định kèm.** Lưu ở đâu (file JSON theo session / SQLite / khác — cân nhắc NFR-5 phụ thuộc tối thiểu) · lưu khi nào (mỗi turn / định kỳ) · hết hạn sau bao lâu · có lưu cả run dở (liên quan D5 phương án c) không.

---

## D7
### Có bao giờ làm verification layer

**Chặn** không chặn gì; xem lại sau Phase 8 · **Mức** P2 · **Liên quan** [ADR-018](02-adr.md#adr-018)

**Trạng thái hiện tại** Đã loại khỏi kiến trúc.

**Điều kiện mở lại.** Dữ liệu Phase 8 cho thấy một lớp sai cụ thể mà (a) eval hồi quy không bắt được, (b) xác nhận người dùng không che được, và (c) mô tả tool tốt hơn không chữa được. Ba điều kiện đồng thời. Nếu không đủ cả ba thì không mở lại.

---

## D8
### Có fine-tune không, base model nào

**Chặn** Phase 10 · **Mức** P2 · **Liên quan** [ADR-015](02-adr.md#adr-015)

**Chỉ trả lời được bằng dữ liệu Phase 8/9.** Nếu SmolLM3 đạt ngưỡng thì câu trả lời là không, và toàn bộ PLAN 1 gốc không cần thực hiện.

**Nếu phải làm, quyết định kèm.** Fine-tune chính SmolLM3 hay đổi base model · nguồn dữ liệu (log thật Phase 8 là ưu tiên một) · LoRA hay QLoRA · hạ tầng compute.

---

## D9
### Con số mục tiêu

**Chặn** Phase 5 (không có ngưỡng thì Phase 5 không có exit criteria) và Phase 8 · **Mức** P0

**Cần chốt.**

| Chỉ số | Cần con số | Ghi chú |
|---|---|---|
| p95 latency cho turn không dùng tool | ? | Đây là trường hợp phổ biến nhất |
| p95 latency cho turn một tool | ? | |
| Per-step tool-selection accuracy (lớp S) | ? | PLAN 1 đề xuất >95%, **chưa validate** |
| End-to-end completion rate (lớp M) | ? | Thấp hơn per-step do lỗi cộng dồn |
| Tỉ lệ gọi tool thừa (lớp N) | ? | Chỉ số trung tâm của vấn đề P1 |
| Tỉ lệ chặn nhầm (lớp S bị cổng chặn) | ? | Hướng sai đối xứng, **bắt buộc có ngưỡng** |
| Tỉ lệ abstain đúng (lớp A) | ? | |

**Cách chốt.** Chạy baseline ở Phase 5 với cổng tắt, xem model tự nhiên làm được gì, rồi đặt mục tiêu cao hơn baseline một mức hợp lý. Đặt mục tiêu trước khi đo là đoán mò; đặt sau khi đo là lười. Cách đúng: đặt **mức tối thiểu chấp nhận được** trước (dưới mức này thì không ship), đặt **mục tiêu** sau khi thấy baseline.

---

## D10
### Số run đồng thời; một `llama_context` hay nhiều

**Chặn** Phase 8; chặn cứng việc mở server cho nhiều client · **Mức** P1 · **Liên quan** [ADR-002](02-adr.md#adr-002)

**Vì sao quan trọng.** `llama_context` không thread-safe và chỉ decode một luồng tại một thời điểm. Cả ba plan nguồn đều bỏ sót điều này.

**Phương án.** (a) Một context, serialize bằng mutex, hàng đợi có giới hạn — đơn giản, throughput = 1. (b) N context, mỗi session một cái — song song được nhưng KV cache nhân N, tốn RAM đáng kể với model 3B và ctx vài nghìn token. (c) Một context + pool session với cơ chế swap KV cache — phức tạp.

**Cần biết trước khi chốt.** Bao nhiêu người dùng đồng thời trong thực tế? Nếu là công cụ cá nhân chạy trên homelab thì (a) là đủ và mọi thứ khác là phức tạp thừa.

---

## D11
### Cơ chế lưu secret cho tool

**Chặn** Phase 7 · **Mức** P1

**Yêu cầu.** Tool nhận credential từ runtime dựa trên tên tool, không bao giờ từ tham số model sinh. Model không thấy tên biến secret. Log che trước khi ghi.

**Phương án.** (a) Biến môi trường của process — đơn giản nhất, hợp với daemon systemd (`EnvironmentFile=`), đủ cho homelab. (b) File cấu hình có phân quyền hệ thống. (c) Secret manager ngoài — thừa với quy mô hiện tại.

**Đề xuất nghiêng về (a)**, nhưng cần chốt để Phase 7 có chỗ đọc.

---

## D12
### Xử lý KV cache khi huỷ giữa chừng

**Chặn** Phase 2 · **Mức** P1

**Vấn đề.** Token sinh dở đã nằm trong KV cache. Nếu không xử lý, turn sau sẽ prefill lên trên một cache chứa token không còn tồn tại trong history.

**Phương án.** (a) Rollback cache về đúng độ dài prefix đã commit — nhanh, cần API cắt cache của llama.cpp (`llama_kv_cache_seq_rm` hoặc tương đương trong phiên bản đang dùng). (b) Đánh dấu cache bẩn, turn sau prefill lại toàn bộ — đơn giản nhưng mất hết lợi ích của ADR-012 ở turn ngay sau khi huỷ.

**Đề xuất (a)**, với (b) làm dự phòng nếu API không có sẵn trong phiên bản llama.cpp đang wrap.

---

## D13
### Một plugin nhiều function — định danh và không gian tên

**Chặn** Phase 3 · **Mức** P2

**Bối cảnh.** Thiết kế đã chốt: tool là plugin trong `plugins/`, mỗi plugin chứa nhiều function.

**Cần quyết định.** Model gọi bằng `{"plugin":"weather","name":"get_current"}` (hai trường) hay `{"name":"weather.get_current"}` (một trường có dấu chấm)? Hai trường rõ ràng hơn cho validator và cho `get_tool_detail`; một trường ngắn hơn và ít token hơn. Trùng tên function giữa hai plugin xử lý thế nào? Có cho phép gọi tắt khi không trùng không (**không nên** — mơ hồ với model)?

**Đề xuất** Hai trường, không cho gọi tắt.

---

## D14
### `ToolSelection` mặc định cho một session mới

**Chặn** Phase 5 · **Mức** P2

**Vì sao quan trọng.** Đây là nơi vấn đề P1 bắt đầu. Nếu mặc định là "bật tất cả tool" thì mọi session đều gánh rủi ro overuse ngay từ turn đầu.

**Phương án.** (a) Bật tất cả — đơn giản, rủi ro cao nhất. (b) Bật tập tối thiểu, ứng dụng tự bật thêm — an toàn nhất nhưng đẩy gánh nặng sang người gọi API. (c) `ToolMode::None` cho turn đầu, chuyển sang `Auto` từ turn thứ hai — kỳ quặc, không có cơ sở. (d) Bật tất cả nhưng dựa vào cổng type-token để lọc — phụ thuộc D1 thành công.

**Ghi chú.** Câu trả lời phụ thuộc kết quả Phase 5. Nếu cổng hoạt động tốt thì (d); nếu không thì (b).

---

## Lịch rà soát

| Khi nào | Rà soát gì |
|---|---|
| Trước Phase 2 | D3, D12 |
| Trước Phase 3 | D2, D13 |
| Trước Phase 5 | D1, D9 (mức tối thiểu chấp nhận được), D14 |
| Trước Phase 6b | D4 |
| Trước Phase 7 | D2 (phần out-of-process), D5, D11 |
| Trước Phase 8 | D6, D9 (mục tiêu), D10 |
| Sau Phase 8 | D7, D8 |