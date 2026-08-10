# Cài bằng AI agent

Nếu bạn đã dùng **Claude Code**, **Codex CLI**, **Antigravity** hay **Gemini CLI**, có thể giao
luôn việc cài Telebit cho agent: nó tự nhận distro, chọn đúng cách cài trong
[Cài đặt](installation.md), chạy lệnh và xác minh lại bằng `telebit doctor`.

Cách này **không cài nhanh hơn** [APT repo](installation.md#apt-repo). Nó hữu ích khi bạn không
chắc distro của mình thuộc nhánh nào, hoặc khi muốn agent xử lý luôn phần sự cố phát sinh.

!!! danger "Agent sẽ chạy lệnh `sudo` trên máy bạn"

    Việc cài bộ gõ đòi hỏi quyền root — agent sẽ xin chạy `apt`, `dnf` hoặc `cmake --install`.

    - **Đọc từng lệnh trước khi duyệt.** Đừng bật chế độ tự duyệt toàn bộ (`--dangerously-skip-permissions`,
      "auto-approve", "YOLO mode"…) cho tác vụ này.
    - Agent có thể **nhớ sai** tên gói hoặc URL. Prompt bên dưới bắt nó đọc README của dự án
      trước khi chạy, nhưng bạn vẫn nên đối chiếu với [Cài đặt](installation.md).
    - Nếu một lệnh trông không liên quan tới việc cài bộ gõ — từ chối và hỏi lại.

## 1. Chuẩn bị agent

| Công cụ | Loại | Khởi động |
|---|---|---|
| [Claude Code](https://claude.com/claude-code) | CLI | `claude` trong terminal |
| [Codex CLI](https://developers.openai.com/codex/cli) | CLI | `codex` trong terminal |
| [Gemini CLI](https://github.com/google-gemini/gemini-cli) | CLI | `gemini` trong terminal |
| [Antigravity](https://antigravity.google/) | IDE | Mở app → panel agent |

!!! note "Cách cài từng công cụ thay đổi theo thời gian"

    Ba CLI ở trên thường cài qua `npm install -g` (`@anthropic-ai/claude-code`,
    `@openai/codex`, `@google/gemini-cli`), còn Antigravity tải về dạng ứng dụng. Tên gói và
    cách cài do từng nhà cung cấp quyết định và có thể đổi — hãy theo trang chính thức ở cột
    đầu bảng thay vì tin vào lệnh chép lại ở đây.

Chạy agent ở **thư mục bất kỳ** cũng được — nó không cần source code của Telebit, trừ khi máy
bạn thuộc nhánh phải build từ source (agent sẽ tự clone).

## 2. Dán prompt này

````text
Cài bộ gõ tiếng Việt Telebit (addon cho fcitx5) lên máy Linux này.

Nguồn chính thức — đọc README ở đây trước khi làm bất cứ gì:
https://github.com/sonnam0904/telebit

Làm theo thứ tự:

1. Xác định distro và phiên bản: `cat /etc/os-release`, và `lsb_release -cs` nếu có.

2. Kiểm tra fcitx5: `command -v fcitx5`. Chưa có thì cài fcitx5 và
   fcitx5-configtool bằng package manager của distro trước.

3. Chọn đúng một cách cài:
   - Ubuntu 22.04 (jammy) / 24.04 (noble) / 26.04 (resolute),
     Debian 12 (bookworm) / 13 (trixie)
     → APT repo của dự án, có thêm khoá GPG (mục "Cách 1" trong README).
   - Ubuntu/Debian có codename khác năm cái trên
     → tải .deb mới nhất từ GitHub Releases, chọn hậu tố gần nhất,
       cài bằng `sudo apt install -y ./<file>.deb`.
   - Fedora / CentOS
     → tải .rpm từ Releases, chọn bản khớp `rpm -E %fedora` (`~fedora43`
       hoặc `~fedora44`), rồi `sudo dnf install -y ./<file>.rpm`.
   - Distro khác (Arch, openSUSE, ...)
     → clone repo, cài dependency, build từ source theo mục "Cách 5".

4. Khởi động lại fcitx5: `fcitx5 -r`.

5. Xác minh. Chạy `telebit doctor` nếu lệnh đó tồn tại (chỉ bản mới mới
   kèm CLI này). Nếu doctor báo dòng ✘, đọc phần gợi ý của nó, sửa, rồi
   chạy lại cho tới khi sạch. Nếu không có lệnh `telebit`, kiểm tra bằng:
   `ls /usr/lib/*/fcitx5/telebit-fcitx5.so /usr/lib64/fcitx5/telebit-fcitx5.so 2>/dev/null`

Quy tắc:
- Giải thích mỗi lệnh trước khi chạy nó.
- Không chạy lệnh xoá/ghi đè ngoài phạm vi việc cài này.
- Không sửa dotfile hay cấu hình cá nhân của tôi, trừ khi việc cài bắt buộc
  và bạn đã nói rõ lý do trước.
- Bước nào fail thì dừng và báo tôi, đừng tự đổi sang cách khác.

Xong thì nhắc tôi: mở fcitx5-configtool → tab Input Method → Add →
chọn "Vietnamese Telex (UTF-8) - Telebit", rồi logout/login nếu chưa thấy.
````

## 3. Sau khi agent chạy xong

Agent cài được gói, nhưng **bước cuối vẫn phải làm tay** — fcitx5 không cho phép thêm input
method từ dòng lệnh:

```bash
fcitx5-configtool
```

Tab **Input Method** → **Add** → tìm `telebit-fcitx5` → **Apply**. Chi tiết và cách xử lý khi
không thấy trong danh sách: [Dùng lần đầu](first-use.md).

Sau đó thử gõ `tieengs vieetj` — phải ra **tiếng việt**.

## Khi agent làm sai

Cách cài này thêm một lớp trung gian giữa bạn và hệ thống, nên khi hỏng thì khó lần hơn. Nếu
kết quả không như mong đợi:

1. Chạy `telebit doctor` (hoặc `telebit doctor --markdown` để lấy bảng dán vào issue). Lệnh này
   đọc trạng thái thật của máy, không phụ thuộc vào việc agent tường thuật đúng hay sai —
   xem [Lệnh telebit doctor](../reference/doctor.md).
2. Đối chiếu những gì agent đã làm với [Cài đặt](installation.md). Sai lệch phổ biến nhất là
   cài nhầm `--user` trong khi fcitx5 đến từ APT.
3. Vẫn tắc thì [mở issue](https://github.com/sonnam0904/telebit/issues/new) — kèm output
   `telebit doctor --markdown` và **cả log lệnh agent đã chạy**.

!!! tip "Muốn tự kiểm soát hoàn toàn?"

    Các cách cài thủ công ở [Cài đặt](installation.md) không dài hơn bao nhiêu, và bạn thấy
    chính xác từng lệnh chạy trên máy mình.
