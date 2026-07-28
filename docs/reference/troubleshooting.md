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

Xem phần cấu hình trong [Trợ lý AI](../ai-assistant.md) — cần `AI_API_KEY` trong biến môi
trường, và biến môi trường phải nhìn thấy được từ tiến trình fcitx5 (đặt trong
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

## Vẫn không xong?

[Mở issue](https://github.com/sonnam0904/telebit/issues/new) kèm:

- Distro + phiên bản, desktop (GNOME/KDE/…), X11 hay Wayland
- Cách cài (APT / `.deb` / `.rpm` / CMake / `install.sh`)
- Kết quả của `fcitx5 --version` và `echo "$GTK_IM_MODULE $QT_IM_MODULE $XMODIFIERS"`
- Chuỗi phím bạn gõ và kết quả nhận được
