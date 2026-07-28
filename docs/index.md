---
title: Trang chủ
---

# Telebit

**Bộ gõ Telex/VNI tiếng Việt cho Linux** — gồm một engine C++ dùng độc lập được, và addon
`telebit-fcitx5` cho fcitx5 trên GNOME/KDE/…

Engine được thiết kế theo **cấu trúc âm tiết tiếng Việt** (*Âm đầu + Vần + Thanh*) chứ không
phải thay thế ký tự đơn thuần, nên nó biết `tieengs` là `tiếng` mà `cheese` vẫn là `cheese`.

<div class="grid cards" markdown>

-   :material-console: **Tôi muốn cài và dùng ngay**

    ---

    Cài bằng một dòng `apt install`, bật fcitx5, chọn input method rồi gõ.

    **Bắt đầu:** [Cài đặt](getting-started/installation.md) → [Dùng lần đầu](getting-started/first-use.md)

-   :material-tune: **Tôi muốn tinh chỉnh cách gõ**

    ---

    Đổi sang VNI, kiểu đặt dấu mới, bật/tắt kiểm tra chính tả, khai báo gõ tắt.

    **Bắt đầu:** [Chế độ gõ](guide/typing-modes.md) → [Tuỳ chọn cấu hình](guide/configuration.md)

-   :material-robot-outline: **Tôi muốn dùng trợ lý AI**

    ---

    ++ctrl+shift+space++ ngay trong lúc gõ để soạn, dịch, tóm tắt, sửa câu.

    **Bắt đầu:** [Trợ lý AI](ai-assistant.md)

-   :material-code-braces: **Tôi muốn build từ source**

    ---

    Core C++17 + CMake, có bộ test riêng. Engine dùng được tách rời khỏi fcitx5.

    **Bắt đầu:** [Build & test](dev/build-and-test.md) → [Kiến trúc](dev/architecture.md)

</div>

## Thử ngay xem nó làm gì

| Gõ | Ra |
|---|---|
| `tieengs vieetj` | tiếng việt |
| `nguyeenx` | nguyễn |
| `Vieetj Nam 2024` | Việt Nam 2024 |
| `person`, `address`, `cheese` | person, address, cheese *(giữ nguyên)* |

Cột thứ ba mới là phần khó: các chữ `s`, `f`, `r`, `x`, `j`, `w` và nguyên âm đôi trong tiếng Anh
đều trùng với phím phụ trong Telex. Telebit kiểm tra âm tiết kết quả có hợp lệ trong tiếng Việt
hay không, nếu không thì khôi phục lại đúng những phím bạn đã gõ — xem
[Kiểm tra chính tả & escape](concepts/spell-check.md).

## Cài nhanh trên Ubuntu/Debian

```bash
curl -fsSL https://sonnam0904.github.io/telebit/pubkey.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/telebit-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/telebit-archive-keyring.gpg] https://sonnam0904.github.io/telebit $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/telebit.list

sudo apt update
sudo apt install telebit
```

Các cách khác (`.deb`, `.rpm`, CMake thủ công, `install.sh`) xem [Cài đặt](getting-started/installation.md).

## Cần giúp?

- Gõ ra sai, không thấy input method, chữ bị nhân đôi → [Xử lý sự cố](reference/troubleshooting.md)
- Đang dùng vnkey cũ → [Chuyển từ vnkey](reference/migrate-from-vnkey.md)
- Lỗi hoặc góp ý → [mở issue trên GitHub](https://github.com/sonnam0904/telebit/issues/new)
