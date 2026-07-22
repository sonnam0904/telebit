# Trợ lý AI trong Telebit

> Quay lại [README](../README.md#cấu-hình-trợ-lý-ai).

Telebit có **trợ lý AI**: mở một ô nhập ngay trong lúc gõ, viết yêu cầu (kèm ngữ cảnh copy được nếu muốn), nhấn Enter để AI sinh văn bản và chèn thẳng vào ô nhập của ứng dụng. Hữu ích cho: soạn/trả lời tin nhắn, dịch, tóm tắt, sửa câu…

## Bật tính năng

Trong `fcitx5-configtool` → **Addons → telebit-fcitx5 → Configure**, bật ô **“Bật trợ lý AI”**. Đây là tuỳ chọn AI **duy nhất** trong configtool — mọi tham số còn lại (API key, endpoint, model, chỉ dẫn hệ thống, số token) đều đọc từ **biến môi trường**.

## Cấu hình bằng biến môi trường

| Biến | Bắt buộc | Mặc định | Ý nghĩa |
|---|---|---|---|
| `AI_API_KEY` | ✅ | (trống) | API key của dịch vụ AI (vd OpenAI `sk-...`). Không có key thì tính năng báo lỗi. |
| `AI_ENDPOINT` | — | `https://api.openai.com/v1/chat/completions` | Địa chỉ API kiểu OpenAI chat/completions. Đổi để dùng Azure OpenAI, groq, hay bản chạy nội bộ. |
| `AI_MODEL` | — | `gpt-4.1-mini` | Tên model. |
| `AI_SYSTEM_PROMPT` | — | (prompt tiếng Việt tự nhiên, súc tích) | Chỉ dẫn hệ thống cho AI. |
| `AI_MAX_TOKENS` | — | `4096` | Số token tối đa cho phản hồi. |
| `AI_TEMPERATURE` | — | `0.3` | Độ "sáng tạo" khi sinh văn bản. Thấp (vd `0.3`) → bám sát chỉ dẫn, ổn định, ít nhả rác — cần cho các model nhỏ chạy nội bộ. Cao (vd `0.7`–`1.0`) → sáng tạo hơn, hợp làm thơ/văn. |

**Tại sao dùng biến môi trường thay vì lưu trong config?**
File cấu hình của fcitx là **văn bản thô, không mã hoá** (`~/.config/fcitx5/conf/telebit-fcitx5.conf`). Nếu để API key ở đó, key dễ lọt vào bản sao lưu, git, ảnh chụp màn hình hay khi chia sẻ cấu hình. Đưa key vào biến môi trường tách bí mật ra khỏi file cấu hình.

**Cách đặt biến môi trường (toàn hệ thống, cho mọi phiên đăng nhập):**

```bash
# Thêm vào /etc/environment (cần sudo). Chỉ AI_API_KEY là bắt buộc.
echo 'AI_API_KEY=sk-...' | sudo tee -a /etc/environment
# (tuỳ chọn) nếu muốn đổi endpoint/model:
echo 'AI_ENDPOINT=https://api.openai.com/v1/chat/completions' | sudo tee -a /etc/environment
echo 'AI_MODEL=gpt-4.1-mini' | sudo tee -a /etc/environment
```

> ⚠️ **Phải ĐĂNG XUẤT rồi ĐĂNG NHẬP LẠI** (hoặc khởi động lại máy) sau khi sửa `/etc/environment`. fcitx5 khởi động theo phiên đăng nhập và chỉ đọc biến môi trường được nạp lúc login; nếu không đăng nhập lại, tiến trình fcitx5 sẽ không thấy `AI_API_KEY`.

Kiểm tra fcitx5 đã nhận biến chưa:

```bash
tr '\0' '\n' < /proc/$(pgrep -x fcitx5)/environ | grep AI_API_KEY
```

Phải thấy dòng `AI_API_KEY=...` có giá trị (không rỗng).

## Cách dùng

| Phím | Tác dụng |
|---|---|
| `Ctrl+Shift+Space` | Mở ô nhập yêu cầu; bấm **lại** để **thoát** (kể cả đang gõ dở) |
| `Ctrl+V` | Nạp nội dung clipboard làm **ngữ cảnh** (`<context>`), gửi kèm yêu cầu |
| `Enter` (hoặc `Ctrl+Enter`) | **Gửi** yêu cầu lên AI; kết quả được chèn vào ô nhập |
| `Esc` | Huỷ / đóng ô |

Ví dụ: copy một đoạn hội thoại → `Ctrl+Shift+Space` → `Ctrl+V` → gõ *“trả lời ngắn, thân mật”* → `Enter`.

> **Cần `wl-clipboard` (Wayland) hoặc `xclip` (X11)** để `Ctrl+V` nạp được ngữ cảnh. Gói `.deb`/`.rpm` đã đặt chúng ở mức *Recommends/Suggests*; nếu cài thủ công, hãy tự cài công cụ tương ứng với môi trường của bạn.

## Skills — hướng dẫn tái sử dụng (`/tên-skill`)

Bạn có thể tạo sẵn các **skill** dưới dạng file Markdown và gọi nhanh trong ô AI. Nội dung file sẽ được đưa vào **chỉ dẫn hệ thống** (system role) để hướng dẫn AI *cách* thực hiện yêu cầu — model làm theo mà không in lại nội dung skill.

1. Tạo một thư mục chứa skill, mỗi skill là một file `.md`, ví dụ:

   ```text
   ~/.config/telebit/skills/
   ├── reply-email.md     # hướng dẫn viết email trả lời
   └── recap-doc.md       # hướng dẫn tóm tắt văn bản
   ```

   Nội dung file là hướng dẫn tự do bằng tiếng Việt (giọng văn, bố cục, ràng buộc…). Ví dụ `reply-email.md`:

   ```markdown
   # Skill: Trả lời email
   Viết email trả lời lịch sự dựa trên email gốc trong <context>.
   - Mở đầu bằng lời chào phù hợp.
   - Trả lời thẳng vào các điểm chính.
   - Kết thúc bằng lời chào và chữ ký ngắn.
   ```

2. Trỏ addon tới thư mục đó: `fcitx5-configtool` → **telebit-fcitx5 → Configure → “Thư mục chứa file skill (.md)”** (hỗ trợ `~`).

3. Dùng trong ô AI: gõ `/tên-skill` ở **đầu** prompt, rồi phần yêu cầu:

   ```text
   [AI] (ngữ cảnh 66 ký tự) /reply-email hãy trả lời email này cho tôi
   ```

   Telebit sẽ nạp `reply-email.md` làm chỉ dẫn hệ thống, kèm ngữ cảnh (`<context>`) và yêu cầu của bạn rồi gửi cho AI. Tên skill chỉ gồm chữ, số, `-`, `_`; nếu file không tồn tại, `/tên` được coi như văn bản thường.

> **Lưu ý riêng tư:** yêu cầu và ngữ cảnh sẽ được **gửi lên dịch vụ AI bên ngoài**. Đừng nhập thông tin nhạy cảm. Một số ứng dụng chat “nuốt” phím Enter (dùng Enter để gửi tin nhắn) — khi đó hãy dùng `Ctrl+Enter` để gửi cho AI.
