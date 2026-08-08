---
title: Bắt đầu
---

# Bắt đầu

Trang này giúp bạn chọn đường đi ngắn nhất tới lúc gõ được tiếng Việt.

## Chọn hướng của bạn

<div class="grid cards" markdown>

-   :material-package-variant-closed: **Tôi chỉ muốn dùng**

    ---

    Phù hợp nếu bạn:

    - Dùng Ubuntu/Debian hoặc Fedora
    - Không muốn build gì cả
    - Muốn tự nhận bản cập nhật sau này

    **Bắt đầu:** [Cài đặt → APT repo](installation.md#apt-repo) → [Dùng lần đầu](first-use.md)

-   :material-hammer-wrench: **Tôi muốn build từ source**

    ---

    Phù hợp nếu bạn:

    - Dùng distro khác (Arch, openSUSE…)
    - Muốn sửa/đóng góp code
    - Muốn dùng riêng engine C++ trong dự án khác

    **Bắt đầu:** [Cài đặt → CMake](installation.md#cmake) → [Build & test](../dev/build-and-test.md)

</div>

## Bạn cần có sẵn gì

Telebit là addon cho **fcitx5**, không phải một bộ gõ độc lập. Trước khi cài, máy cần:

- **fcitx5** đang chạy và là input method của session (không phải IBus)
- Nếu build từ source: compiler **C++17**, **CMake ≥ 3.21**, header của fcitx5 và libcurl

Nếu chưa từng dùng fcitx5, [Dùng lần đầu](first-use.md) có phần hướng dẫn đặt fcitx5 làm
input method mặc định.

!!! warning "Đang dùng vnkey?"

    Gỡ vnkey trước khi cài Telebit, nếu không hai bộ gõ sẽ tranh nhau xử lý phím.
    Xem [Chuyển từ vnkey](../reference/migrate-from-vnkey.md).

## Tiếp theo là gì?

Sau khi gõ được tiếng Việt:

1. **[Chế độ gõ](../guide/typing-modes.md)** — hiểu preedit gạch chân so với direct commit, chọn cái phù hợp với ứng dụng bạn dùng
2. **[Tuỳ chọn cấu hình](../guide/configuration.md)** — VNI, kiểu đặt dấu, tự viết hoa, phím tắt bật/tắt tiếng Việt
3. **[Cách gõ Telex & VNI](../guide/telex-vni.md)** — bảng phím phụ đầy đủ và cách gõ thoát khi engine đoán sai
4. **[Trợ lý AI](../ai-assistant.md)** — soạn/dịch/tóm tắt văn bản ngay trong ô nhập

## Cần giúp?

- Cài xong mà không thấy `telebit-fcitx5` trong danh sách → [Xử lý sự cố](../reference/troubleshooting.md)
- Muốn gỡ → [Gỡ cài đặt](../reference/uninstall.md)
- [Mở issue trên GitHub](https://github.com/sonnam0904/telebit/issues/new)
