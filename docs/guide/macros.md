# Gõ tắt (macro)

Khai báo cặp **viết tắt → nội dung**. Khi bạn kết thúc từ (Space, Enter hoặc dấu câu), từ viết
tắt được thay bằng nội dung đầy đủ.

## Khai báo

`fcitx5-configtool` → **Addons** → **telebit-fcitx5** → **Configure** → mục **Macros**
(*“Gõ tắt: từ viết tắt sẽ được thay bằng nội dung khi kết thúc từ”*).

Bấm thêm dòng, mỗi dòng có hai ô:

| Ô | Nhãn trong configtool | Ví dụ |
|---|---|---|
| **Abbrev** | Từ viết tắt (vd: `vn`) | `vn` |
| **Expansion** | Nội dung thay thế (vd: Việt Nam) | `Việt Nam` |

Gõ `vn` rồi Space → ra `Việt Nam `.

## Ví dụ hữu ích

| Abbrev | Expansion |
|---|---|
| `vn` | Việt Nam |
| `tphcm` | Thành phố Hồ Chí Minh |
| `sdt` | 0912 345 678 |
| `em1` | ten.cua.ban@example.com |
| `kg` | Kính gửi |
| `tct` | Trân trọng cảm ơn, |

## Cách hoạt động

- Đối chiếu diễn ra trên **phím thô** bạn đã gõ, **không phân biệt hoa/thường** — nên `VN`, `Vn`,
  `vn` đều khớp cùng một macro.
- Nội dung thay thế được chèn **nguyên văn**, không đi qua bộ chuyển Telex. Vì vậy Expansion
  hãy viết sẵn tiếng Việt có dấu (`Việt Nam`), không viết kiểu Telex (`Vieetj Nam`).
- Macro chỉ nổ khi **kết thúc từ**. Đang gõ dở thì chưa thay.

!!! tip "Chọn từ viết tắt không trùng từ thật"

    `vn` an toàn vì không phải âm tiết tiếng Việt và cũng không phải từ tiếng Anh. Tránh đặt
    abbrev trùng từ bạn hay gõ, ví dụ đặt `la` → `Là` sẽ làm bạn không gõ được chữ `la`.

---

## Ép dùng preedit cho từng ứng dụng { #force-preedit }

Cùng trang Configure còn có mục **ForcePreeditApps** (*“Danh sách ứng dụng sử dụng telebit”*) —
không liên quan gõ tắt, nhưng hay dùng chung nên ghi ở đây.

Dùng khi [DirectCommitRollback](typing-modes.md) đang bật nhưng **một ứng dụng cụ thể** hiển thị
lỗi. Thêm ứng dụng đó vào danh sách để riêng nó quay về chế độ preedit gạch chân, các app còn lại
không đổi.

| Ô | Nhãn trong configtool | Ví dụ |
|---|---|---|
| **Program** | Tên ứng dụng, ví dụ: firefox / chromium / code | `firefox` |
| **Enabled** | Ép dùng preedit mode cho ứng dụng này | ✅ |

Mặc định danh sách có sẵn **`firefox`** đã bật.

!!! note "Lấy tên ứng dụng ở đâu?"

    Là tên tiến trình mà fcitx5 nhìn thấy (`firefox`, `chromium`, `code`, `alacritty`…). Addon
    tự ghi nhận các ứng dụng bạn đã dùng vào file config, nên khi mở configtool bạn thường thấy
    sẵn tên cần chọn trong danh sách.

## Liên quan

- [Tuỳ chọn cấu hình](configuration.md) — toàn bộ các ô khác
- [Chế độ gõ](typing-modes.md) — preedit so với direct commit
