# Chế độ gõ

Telebit có hai cách hiển thị chữ đang gõ. Cả hai cho ra kết quả cuối giống nhau — khác nhau ở
chỗ bạn **thấy** gì trong lúc gõ.

Đổi trong `fcitx5-configtool` → tab **Addons** → **telebit-fcitx5** → **Configure** →
ô **DirectCommitRollback**.

## So sánh nhanh

| | Preedit (gạch chân) | Direct commit + rollback |
|---|---|---|
| **DirectCommitRollback** | Tắt | Bật *(mặc định)* |
| Chữ hiện ở đâu | Vùng preedit tạm, có gạch chân | Trực tiếp trong ô nhập |
| Chốt vào ứng dụng khi | Nhấn Space/Enter/dấu câu | Ngay từng phím |
| Tương thích | Rất rộng | Tốt với hầu hết app, vài app có vấn đề |
| Undo/Redo của app | Bình thường | Có thể hơi khác |

## Preedit (gạch chân)

Ký tự đang gõ nằm trong vùng preedit **có gạch chân**. Khi bạn nhấn Space, Enter hoặc dấu câu,
cả từ được chốt và gửi vào ứng dụng.

Ưu điểm: ổn định, tương thích tốt với gần như mọi ứng dụng — editor, terminal, trình duyệt.

Chọn chế độ này nếu bạn thấy chữ nhảy loạn, undo hoạt động lạ, hoặc dùng nhiều terminal/IDE
khó tính.

## Direct commit + rollback

Chữ Việt xuất hiện **trực tiếp** trong ô nhập, không có gạch chân. Gõ `nguyeenx` thì bạn thấy
nó biến đổi dần thành `nguyễn` ngay tại chỗ.

Cách hoạt động: mỗi phím mới, addon xoá phần đã gửi rồi gửi lại kết quả mới — gọi là *rollback*.

!!! note "Tự động quay về preedit"

    Một số ứng dụng không hỗ trợ đầy đủ khả năng xoá-lùi của fcitx5. Với các app đó,
    `telebit-fcitx5` **tự phát hiện và chuyển về chế độ preedit** để tránh lỗi hiển thị — bạn
    không cần làm gì.

!!! warning "Undo/Redo"

    Vì chữ được gửi rồi xoá liên tục, ++ctrl+z++ trong một số ứng dụng sẽ lùi theo từng bước
    rollback thay vì từng từ. Nếu thấy khó chịu, **tắt DirectCommitRollback**.

## Nên chọn cái nào?

Mặc định (**bật**) phù hợp với phần lớn người dùng: thấy chữ Việt ngay, không có gạch chân lạ mắt.

Tắt nó khi:

- Undo/Redo trong app bạn dùng bị rối
- Chữ nhảy, nhân đôi, hoặc mất ký tự
- Bạn gõ nhiều trong terminal, ứng dụng Electron cũ, hoặc app remote desktop

## Liên quan

- [Tuỳ chọn cấu hình](configuration.md) — các ô còn lại trong cùng trang Configure
- [Xử lý sự cố → chữ bị nhân đôi](../reference/troubleshooting.md#chu-bi-nhan-doi)
