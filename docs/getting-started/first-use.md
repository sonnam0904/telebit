# Dùng lần đầu

Cài xong gói là mới xong một nửa. Còn hai việc: đảm bảo **fcitx5 đang điều khiển bàn phím**,
và **thêm Telebit vào danh sách input method**.

## Bước 1 — Đặt fcitx5 làm input method của session

**Telebit đã làm sẵn bước này.** Gói cài một drop-in
`/usr/lib/environment.d/60-telebit-fcitx5.conf` (bản `install.sh --user` thì đặt ở
`~/.config/environment.d/`) khai báo:

```ini
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

systemd đọc file này lúc khởi tạo user session, nên **phải logout/login** (hoặc reboot)
thì mới có tác dụng. Kiểm tra sau khi login lại:

```bash
echo "$GTK_IM_MODULE $QT_IM_MODULE $XMODIFIERS"   # -> fcitx fcitx @im=fcitx
fcitx5 -d
```

!!! warning "Nếu biến vẫn trống sau khi login lại"

    Session của bạn có thể không chạy qua systemd user manager, hoặc `/etc/environment`
    đang đè lên (file `99-environment.conf` sắp sau `60-…` nên thắng). Khi đó dùng cách
    truyền thống: `im-config -n fcitx5` trên Ubuntu/Debian, hoặc `export` ba biến trên
    trong `~/.profile` / `~/.xprofile`.

## Bước 2 — Thêm Telebit vào danh sách input method

1. Mở công cụ cấu hình:

    ```bash
    fcitx5-configtool
    ```

2. Tab **Input Method** → **Add** → tìm `telebit-fcitx5`.

    Tên hiển thị đầy đủ: **“Vietnamese Telex (UTF-8) - Telebit (telebit-fcitx5)”**

3. Thêm vào danh sách → **Apply**.

## Bước 3 — Thử gõ

Chuyển sang `telebit-fcitx5` bằng phím tắt của fcitx5 (thường là ++ctrl+space++), rồi gõ:

| Gõ | Phải ra |
|---|---|
| `tieengs vieetj` | tiếng việt |
| `nguyeenx` | nguyễn |
| `Vieetj Nam` | Việt Nam |

Ra đúng cả ba là xong.

!!! tip "Cần gõ một đoạn tiếng Anh dài?"

    Nhấn ++ctrl+shift+z++ để tạm tắt tiếng Việt, nhấn lại để bật. Đổi được phím này trong
    [Tuỳ chọn cấu hình](../guide/configuration.md).

    Với từ tiếng Anh lẻ thì thường không cần — engine tự nhận ra `person`, `address`,
    `cheese`… không phải âm tiết tiếng Việt và giữ nguyên. Xem
    [Kiểm tra chính tả & escape](../concepts/spell-check.md).

## Không chạy?

| Triệu chứng | Xem |
|---|---|
| Không thấy `telebit-fcitx5` khi bấm Add | [Xử lý sự cố → addon không xuất hiện](../reference/troubleshooting.md#addon-khong-xuat-hien) |
| Gõ ra tiếng Anh, không thành tiếng Việt | [Xử lý sự cố → không chuyển đổi](../reference/troubleshooting.md#khong-chuyen-doi) |
| Chữ bị nhân đôi hoặc nhảy loạn | [Xử lý sự cố → chữ bị nhân đôi](../reference/troubleshooting.md#chu-bi-nhan-doi) |

## Tiếp theo

- [Chế độ gõ](../guide/typing-modes.md) — preedit gạch chân hay hiện chữ trực tiếp
- [Tuỳ chọn cấu hình](../guide/configuration.md) — VNI, kiểu đặt dấu, gõ tắt
- [Trợ lý AI](../ai-assistant.md) — ++ctrl+shift+space++
