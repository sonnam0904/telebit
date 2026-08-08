# Xử lý sự cố

## Không thấy `telebit-fcitx5` trong danh sách input method { #addon-khong-xuat-hien }

Theo thứ tự khả năng cao → thấp:

**1. Chưa logout/login sau khi cài**

fcitx5 quét danh sách addon khi khởi động session. Lần cài đầu tiên gần như luôn cần:

```bash
fcitx5 -r          # thử cái này trước
```

Không được thì **logout/login**, hoặc reboot.

**2. Đã cài user-local nhưng fcitx5 đến từ APT**

Đây là nguyên nhân phổ biến nhất khi `fcitx5 -r` không giúp gì. fcitx5 chỉ nạp addon `.so` từ
thư mục lib addon của **bản build chính nó** — thường `/usr/lib/<arch>/fcitx5`. Nó **không** dò
`$HOME/.local/lib/fcitx5`.

Kiểm tra file đã nằm đúng chỗ:

```bash
ls -l /usr/lib/*/fcitx5/telebit-fcitx5.so /usr/lib/fcitx5/telebit-fcitx5.so 2>/dev/null
ls -l "$HOME/.local/lib/fcitx5/telebit-fcitx5.so" 2>/dev/null
```

Nếu chỉ thấy ở `$HOME/.local` → [cài lại toàn hệ thống](../getting-started/installation.md#cmake).

**3. Thiếu file mô tả addon**

```bash
ls -l /usr/share/fcitx5/addon/telebit-fcitx5.conf \
      /usr/share/fcitx5/inputmethod/telebit-fcitx5.conf
```

Thiếu một trong hai thì bản cài bị lỗi — cài lại.

---

## Gõ ra tiếng Anh, không thành tiếng Việt { #khong-chuyen-doi }

**1. fcitx5 không điều khiển bàn phím**

Addon cài đúng nhưng ứng dụng vẫn nói chuyện với IBus. Kiểm tra:

```bash
echo "$GTK_IM_MODULE $QT_IM_MODULE $XMODIFIERS"
```

Phải ra `fcitx fcitx @im=fcitx`. Nếu không, xem
[Dùng lần đầu → Bước 1](../getting-started/first-use.md).

**2. Chưa chuyển sang Telebit**

fcitx5 có thể đang ở input method khác. Dùng phím tắt chuyển (thường ++ctrl+space++) và để ý
icon trên khay hệ thống.

**3. Đang tắt tiếng Việt**

Bạn có thể đã nhấn ++ctrl+shift+z++ lúc nào đó. Nhấn lại.

**4. fcitx5 chưa chạy**

```bash
pgrep -a fcitx5 || fcitx5 -d
```

---

## Chữ bị nhân đôi, nhảy loạn, hoặc mất ký tự { #chu-bi-nhan-doi }

Gần như luôn do chế độ **direct commit** với một ứng dụng không hỗ trợ đầy đủ xoá-lùi.

Chữa cho **một ứng dụng** (giữ direct commit cho các app khác): thêm app đó vào
[ForcePreeditApps](../guide/macros.md#force-preedit).

Chữa cho **toàn bộ**: tắt **DirectCommitRollback** —
`fcitx5-configtool` → **Addons** → **telebit-fcitx5** → **Configure**.
Xem [Chế độ gõ](../guide/typing-modes.md).

---

## Undo (++ctrl+z++) hoạt động lạ

Cũng là hệ quả của direct commit: chữ được gửi rồi xoá liên tục nên undo lùi theo từng bước
rollback. Tắt **DirectCommitRollback**.

---

## Một từ tiếng Anh bị biến thành tiếng Việt

Từ đó tình cờ tạo ra âm tiết hợp lệ (`data` → `dât`). Gõ lặp phím phụ để huỷ (`dataa` → `data`).

Đầy đủ ở [Kiểm tra chính tả & escape](../concepts/spell-check.md).

Nếu bạn gõ lẫn tiếng Anh rất nhiều, cân nhắc [chế độ VNI](../guide/telex-vni.md#vni) — phím phụ
là chữ số nên không bao giờ va vào chữ cái.

---

## Dấu đặt sai chỗ (`hóa` vs `hoá`)

Cả hai đều đúng chính tả. Đổi quy ước bằng
[ModernToneStyle](../guide/configuration.md#modern-tone).

---

## Trợ lý AI không phản hồi

Xem phần cấu hình trong [Trợ lý AI](../ai-assistant.md) — cần một key trong biến môi
trường (`AI_API_KEY` cho OpenAI, hoặc `ANTHROPIC_API_KEY` / `CLAUDE_CODE_OAUTH_TOKEN`
cho Claude), và biến môi trường phải nhìn thấy được từ tiến trình fcitx5 (đặt trong
`/etc/environment` rồi logout/login, không phải chỉ `export` trong terminal).

Bật log để xem lỗi:

```bash
TELEBIT_AI_DEBUG=1 fcitx5 -r
```

Rồi xem `journalctl --user -f` hoặc stderr của fcitx5.

!!! warning "Log chứa dữ liệu riêng tư"

    `TELEBIT_AI_DEBUG` in ra cả prompt, nội dung clipboard đã dán và phản hồi của API. Chỉ bật
    khi đang chẩn đoán, và đừng dán log lên chỗ công khai.

---

## Chỉ một vài ứng dụng không gõ được { #app-cu-the-khong-go-duoc }

Gõ bình thường ở hầu hết chỗ nhưng riêng một app (thường là Snap / Flatpak) thì không —
đây là ca mà [`telebit doctor`](doctor.md) sinh ra để xử lý.

Tự làm thì chạy hai lệnh này rồi đối chiếu với [bảng "Gặp lỗi thì làm gì"](doctor.md#gap-loi-thi-lam-gi):

```bash
telebit doctor
telebit doctor --deep     # khi app đó lỗi mà app khác cùng runtime vẫn ổn
```

### Nhờ AI agent đọc hộ

Nếu bạn đã dùng **Claude Code**, **Codex CLI**, **Antigravity** hay **Gemini CLI**, có thể để
agent chạy doctor, đọc bảng và sửa. `telebit doctor --markdown` in ra bảng không màu, không có
ANSI escape — đúng dạng agent parse được.

!!! tip "Thay `<TÊN ỨNG DỤNG>` trước khi gửi"

    Prompt bên dưới chỉ hiệu quả khi bạn nói rõ app nào đang lỗi. Không có thông tin đó, agent
    sẽ đi sửa mọi dòng `!` trong báo cáo — phần lớn không phải nguyên nhân.

````text
Máy tôi cài bộ gõ tiếng Việt Telebit (addon cho fcitx5). Gõ được ở phần lớn
ứng dụng, nhưng ứng dụng <TÊN ỨNG DỤNG> thì không. Tìm nguyên nhân giúp tôi.

Công cụ chẩn đoán có sẵn trên máy: `telebit doctor`. Tài liệu:
https://sonnam0904.github.io/telebit/reference/doctor/

Làm theo thứ tự:

1. Chạy `telebit doctor --markdown` và đọc toàn bộ bảng. Nếu không có lệnh
   `telebit`, nghĩa là bản đang cài quá cũ — báo tôi nâng cấp rồi dừng.

2. Xác định ứng dụng đang lỗi thuộc loại nào: `flatpak list --app`,
   `snap list`, hay là app cài thẳng trên máy.

3. Nếu nó là Flatpak/Snap và bảng "Ứng dụng sandbox" chưa đủ kết luận, chạy
   `telebit doctor --deep --markdown`. Lệnh này mở shell bên trong từng
   sandbox để đọc biến môi trường thật, tốn vài giây mỗi app.

4. Chỉ sửa đúng những gì doctor chỉ ra. Cuối báo cáo doctor đã in sẵn danh
   sách gợi ý — ưu tiên làm theo đó thay vì tự nghĩ cách khác.

Cách đọc bảng, để không sửa nhầm chỗ:
- `✘` = đang hỏng thật. Xử lý trước tiên.
- `!` = rủi ro, CHƯA CHẮC là nguyên nhân. Ví dụ dòng "GTK4 thiếu" ở một
  platform snap không có nghĩa app của tôi hỏng — app GTK3 trên cùng runtime
  đó vẫn gõ bình thường. Chỉ đụng tới nếu app đang lỗi thật sự dùng toolkit đó.
- `·` = chỉ là thông tin, không phải kết quả phân tích.
- `n/a` = runtime không hề có toolkit đó, không có gì để sửa. Khác với `thiếu`.

Có trường hợp KHÔNG sửa được: nếu runtime của snap/flatpak đó không có module
fcitx cho toolkit mà app dùng, không có cách nào ép module vào. Gặp ca đó thì
nói thẳng với tôi và đề xuất dùng bản .deb/.rpm của ứng dụng, hoặc chuyển sang
phiên Wayland — đừng thử vòng vo các cách không liên quan.

Quy tắc:
- Giải thích mỗi lệnh trước khi chạy; lệnh nào cần sudo thì nói rõ vì sao.
- Sau mỗi lần sửa, chạy lại `telebit doctor` để kiểm chứng, RỒI bảo tôi thử gõ
  lại trong đúng ứng dụng đó. Doctor xanh hết không thay được việc thử thật.
- Nhiều thay đổi chỉ có hiệu lực sau `fcitx5 -r`, một số cần logout/login.
  Nói rõ cái nào cần gì.
- Sau hai vòng vẫn không ra thì dừng, tổng hợp output `telebit doctor --markdown`
  và những gì đã thử, để tôi mở issue.
````

!!! warning "Agent không thay được bước thử gõ"

    Doctor chỉ kiểm tra được *đường đi* của input method — module có mặt, frontend có mở,
    biến môi trường có tới nơi. Nó không gõ thử hộ bạn. Báo cáo sạch mà app vẫn không gõ được
    là thông tin có ích: [mở issue](https://github.com/sonnam0904/telebit/issues/new) kèm
    output `telebit doctor --markdown`.

---
## Vẫn không xong?

[Mở issue](https://github.com/sonnam0904/telebit/issues/new) kèm:

- Distro + phiên bản, desktop (GNOME/KDE/…), X11 hay Wayland
- Cách cài (APT / `.deb` / `.rpm` / CMake / `install.sh`)
- Kết quả của `fcitx5 --version` và `echo "$GTK_IM_MODULE $QT_IM_MODULE $XMODIFIERS"`
- Chuỗi phím bạn gõ và kết quả nhận được
