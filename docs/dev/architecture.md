# Kiến trúc

Telebit gồm hai phần tách rời:

- **Core C++** — hàm `telex_to_unicode()`, dùng độc lập được, không phụ thuộc fcitx5
- **Addon fcitx5** — `telebit-fcitx5/`, bọc core lại thành input method

## Cây source

| File | Vai trò |
|---|---|
| `vietnamese.h/.cpp` | Entry point `telex_to_unicode()` — glue, xử lý theo từng từ |
| `engine.h/.cpp` | `EngineVietCpp` — quản lý buffer theo từng phím, addon dùng lớp này |
| `canonicalize.h/.cpp` | Escape rules, tách âm đầu/vần, chuẩn hoá vị trí `w`/`aa`/`ee`/`oo`, trích thanh |
| `rime_table.h/.cpp` | Bảng vần + vị trí nguyên âm chính để đặt dấu |
| `render_utf8.h/.cpp` | Render biểu diễn nội bộ → UTF-8, áp dấu, giữ hoa/thường |
| `telebit-fcitx5/` | Addon fcitx5 (preedit, direct commit, config, trợ lý AI) |
| `tests.cpp` | Bộ test của core |

## Luồng chuyển đổi

`telex_to_unicode()` tách chuỗi thành từ, rồi mỗi từ đi qua:

```
từ thô
  → kiểm tra có đủ điều kiện chuyển đổi (chỉ chữ; số chỉ tính khi bật VNI)
  → applyEscapeRules()        huỷ phím phụ khi gõ lặp (ss, aaa, ddd, ww, …)
  → extractTone()             tách thanh ra khỏi thân từ
  → normalizeTripleVowels()   gộp nguyên âm ba của tiếng Anh
  → splitOnsetRime()          tách âm đầu / vần
  → canonicalizeRimeByTable() chuẩn hoá vần theo bảng
  → applyShapesRime()         quy về biểu diễn nội bộ (â='B', ơ='Q', ư='U', …)
  → renderRimeUtf8()          đặt dấu, xuất UTF-8
  → isValidSyllable()         cổng kiểm tra chính tả — không hợp lệ thì trả về phím gốc
```

Biểu diễn nội bộ dùng chữ hoa làm ký hiệu cho nguyên âm có dấu phụ, để phần canonicalize làm việc
trên ASCII một byte: `B`=â, `A`=ă, `E`=ê, `O`=ô, `Q`=ơ, `U`=ư, `D`=đ.

### Vài điểm đáng chú ý

- **Phím phụ đặt ở đâu cũng được** — `extractTone()` gom phím thanh ở mọi vị trí sau nguyên âm
  đầu, phím cuối cùng thắng. Nhờ đó `vieetj` và `vietj` cho cùng kết quả.
- **`gif` là ca đặc biệt** — `gi` cũng là âm đầu hợp lệ, nên parser tổng quát sẽ coi cả `gi` là
  âm đầu và mất thanh. Xử lý riêng để ra `gì`.
- **Vần gõ dở vẫn chuyển** — `isValidSyllable()` dùng tập tiền tố tính trước
  (`getRimeMainVowelPrefixSet()`) nên `tiee` → `tiê` trên đường tới `tiên`, không phải chờ gõ xong.
- **Cổng chính tả có thể từ chối** — nếu kết quả không phải âm tiết hợp lệ, hàm trả về **nguyên
  văn phím đã gõ**. Đây là cơ chế đằng sau [Kiểm tra chính tả](../concepts/spell-check.md).

## Addon fcitx5

`EngineVietCpp` giữ buffer phím thô; addon quyết định hiển thị thế nào:

- **Preedit** — buffer hiện dưới dạng preedit gạch chân, chốt khi gặp Space/Enter/dấu câu
- **Direct commit + rollback** — mỗi phím, addon xoá phần đã gửi rồi gửi lại kết quả mới

Trạng thái là **theo từng input context** (`TelebitInputState`), nên mỗi cửa sổ/ô nhập có buffer
riêng, không lẫn nhau.

Addon còn giữ: macro (khớp trên phím thô, không phân biệt hoa/thường), danh sách ứng dụng ép dùng
preedit, cờ tự viết hoa đầu câu, và trạng thái của ô nhập trợ lý AI.


## Liên quan

- [Build & test](build-and-test.md)
- [Âm tiết tiếng Việt](../concepts/vietnamese-syllable.md) — mô hình ngôn ngữ mà engine dựa vào
