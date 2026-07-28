# Dùng lần đầu

Cài xong gói là mới xong một nửa. Còn hai việc: đảm bảo **fcitx5 đang điều khiển bàn phím**,
và **thêm Telebit vào danh sách input method**.

## Bước 1 — Đặt fcitx5 làm input method của session

Bỏ qua bước này nếu bạn đã dùng fcitx5 cho ngôn ngữ khác.

!!! warning "Nếu bỏ qua bước này"

    Addon vẫn cài thành công nhưng **không có tác dụng gì** — ứng dụng vẫn nói chuyện với IBus
    hoặc input method khác, phím của bạn không đi qua Telebit.

=== "Ubuntu/Debian"

    ```bash
    im-config -n fcitx5
    ```

=== "Cấu hình tay (GNOME/KDE)"

    Thêm vào `~/.profile`, `~/.xprofile` hoặc file env của desktop:

    ```bash
    export GTK_IM_MODULE=fcitx
    export QT_IM_MODULE=fcitx
    export XMODIFIERS=@im=fcitx
    ```

Sau đó **logout/login** (hoặc reboot), rồi kiểm tra fcitx5 đang chạy:

```bash
fcitx5 -d
```

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
