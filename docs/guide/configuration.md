# Tuỳ chọn cấu hình

Mở bằng: `fcitx5-configtool` → tab **Addons** → **telebit-fcitx5** → **Configure**.

## Bảng tuỳ chọn

| Tuỳ chọn | Mặc định | Ý nghĩa |
|---|---|---|
| **DirectCommitRollback** | Bật | Hiện chữ Việt trực tiếp trong ô nhập thay vì preedit gạch chân. Xem [Chế độ gõ](typing-modes.md). |
| **SpellCheckRestore** | Bật | Nếu từ vừa gõ không phải âm tiết tiếng Việt hợp lệ thì khôi phục về đúng phím gốc — giúp gõ xen tiếng Anh (`person`, `address`, `cheese`) không bị biến dạng. Xem [Kiểm tra chính tả](../concepts/spell-check.md). |
| **VNIMode** | Tắt | Chuyển sang kiểu gõ VNI (dùng số thay chữ). Xem [Cách gõ Telex & VNI](telex-vni.md#vni). |
| **ModernToneStyle** | Tắt | Kiểu đặt dấu mới cho vần `oa/oe/uy`. Xem [dưới đây](#modern-tone). |
| **AutoCapitalizeSentence** | Bật | Tự viết hoa chữ đầu câu sau khi gõ `.`, `?`, `!` rồi Space/Enter. |
| **ToggleVietnameseKey** | ++ctrl+shift+z++ | Tạm bật/tắt gõ tiếng Việt. |
| **Macros** | (trống) | Gõ tắt. Xem [Gõ tắt (macro)](macros.md). |

---

## ModernToneStyle — kiểu đặt dấu { #modern-tone }

Với các vần `oa`, `oe`, `uy` không có âm cuối, tiếng Việt có hai quy ước đặt dấu và **cả hai đều
được coi là đúng**:

| Gõ | Tắt *(mặc định — kiểu cũ)* | Bật *(kiểu mới)* |
|---|---|---|
| `hoas` | h**ó**a | ho**á** |
| `khoer` | kh**ỏ**e | kho**ẻ** |
| `thuys` | th**ú**y | thu**ý** |

Kiểu cũ đặt dấu trên nguyên âm đầu, kiểu mới đặt trên nguyên âm sau.

Vần **có âm cuối thì không ảnh hưởng** — `hoanfg` luôn ra `hoàng`, `ngoaij` luôn ra `ngoại`,
bất kể tuỳ chọn này.

---

## AutoCapitalizeSentence

Bật (mặc định): sau khi gõ `.`, `?` hoặc `!` rồi nhấn Space hoặc Enter, chữ cái đầu của từ tiếp
theo được tự viết hoa.

Tắt nó nếu bạn gõ nhiều nội dung mà việc tự viết hoa gây phiền — ví dụ code, tên file có dấu
chấm, hoặc danh sách viết tắt.

---

## ToggleVietnameseKey

Mặc định ++ctrl+shift+z++. Nhấn để **tạm tắt** gõ tiếng Việt, nhấn lại để bật.

Hữu ích khi cần gõ liền một đoạn dài tiếng Anh, tên biến, hoặc câu lệnh terminal.

!!! tip "Thường thì bạn không cần dùng"

    Với **từ** tiếng Anh lẻ, `SpellCheckRestore` đã tự lo. Phím này dành cho trường hợp gõ cả
    **đoạn** — hoặc khi bạn muốn chắc chắn không có gì bị biến đổi.

Khi bạn tắt tiếng Việt giữa lúc đang gõ một từ, phần đang gõ dở được chốt lại trước, không bị mất.

---

## Macros

Xem trang riêng: [Gõ tắt (macro)](macros.md).

---

## Liên quan

- [Chế độ gõ](typing-modes.md) — chi tiết về DirectCommitRollback
- [Cách gõ Telex & VNI](telex-vni.md) — chi tiết về VNIMode
- [Kiểm tra chính tả & escape](../concepts/spell-check.md) — chi tiết về SpellCheckRestore
- [Trợ lý AI](../ai-assistant.md) — cấu hình bằng biến môi trường, không nằm trong trang này
