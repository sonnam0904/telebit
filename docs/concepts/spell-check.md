# Kiểm tra chính tả & escape

Trang này giải thích vì sao gõ `cheese` ra `cheese` chứ không thành `chêse`, và làm gì khi engine
đoán sai.

## Vấn đề

Trong Telex, các chữ `s` `f` `r` `x` `j` `w` và nguyên âm đôi `aa` `ee` `oo` là **phím phụ**.
Nhưng chúng cũng là chữ cái bình thường của tiếng Anh. Nếu áp dụng Telex một cách máy móc:

| Gõ | Telex thô sẽ ra | Đúng phải là |
|---|---|---|
| `person` | pé‌on | person |
| `address` | (biến dạng) | address |
| `cheese` | chêse | cheese |
| `book` | bôk | book |

## Cách Telebit xử lý: kiểm tra âm tiết

Tuỳ chọn **SpellCheckRestore** (bật mặc định) hoạt động như sau: sau khi áp dụng Telex, engine
kiểm tra kết quả có phải **âm tiết tiếng Việt hợp lệ** không — dựa trên bảng âm đầu và bảng vần
(xem [Âm tiết tiếng Việt](vietnamese-syllable.md)). Nếu **không** hợp lệ, nó khôi phục lại đúng
những phím bạn đã gõ.

Nhờ vậy:

```
person → person      address → address     cheese → cheese
book → book          food → food           kangaroo → kangaroo
toolbox → toolbox    employee → employee   zoo → zoo
```

Mà tiếng Việt vẫn chuyển bình thường, kể cả khi đang gõ dở:

```
tieengs → tiếng      nguyeenx → nguyễn     tiee → tiê  (đang trên đường tới "tiên")
```

!!! note "Tắt SpellCheckRestore thì sao?"

    Engine áp dụng Telex thô, không kiểm tra gì. `person` sẽ thành `péon`. Chỉ tắt nếu bạn gõ
    thuần tiếng Việt và muốn hành vi tuyệt đối máy móc, dễ đoán.

## Khi engine vẫn đoán sai: gõ thoát

Kiểm tra chính tả không hoàn hảo — có những từ tiếng Anh **tình cờ tạo ra âm tiết tiếng Việt
hợp lệ**, và khi đó engine không có cách nào biết bạn muốn gì:

| Gõ | Ra | Vì `dât`/`nân` là âm tiết hợp lệ |
|---|---|---|
| `data` | dât | chữ `a` cuối áp mũ lên `a` trước đó |
| `naan` | nân | `aa` = â |

Cách chữa: **gõ lặp phím phụ** để huỷ nó.

### Bảng gõ thoát

| Gõ | Ra | Ghi chú |
|---|---|---|
| `ss` `ff` `rr` `xx` `jj` | `s` `f` `r` `x` `j` | Huỷ thanh — `soffa` → sofa |
| `aaa` | `aa` | `baaad` → baad |
| `eee` | `ee` | `leeech` → leech |
| `ooo` | `oo` | `cooool` → coool |
| `ddd` | `dd` | `edddy` → eddy |
| `ww` | `w` | `sunwworld` → sunworld |
| `…aa` ở cuối từ | huỷ mũ | `dataa` → data |

### Trường hợp `aa` ở cuối từ

Telex cho phép phím phụ đứng **sau âm cuối** — nên `data` được hiểu là `dât`. Gõ thêm một chữ
`a` nữa để huỷ:

```
data  → dât
dataa → data     ✅
```

Rule này **chỉ áp dụng cho chữ `a`**, và chỉ khi `aa` nằm **cuối** những gì bạn đã gõ. Lý do:
tiếng Anh cực hiếm từ có hai chữ `a` liền nhau, trong khi `ee` và `oo` thì đầy (`coffee`,
`agree`, `zoo`) nên áp dụng tương tự sẽ phá nhiều hơn là sửa.

Vì thế các từ *có chứa* `aa` nhưng không ở cuối vẫn an toàn:

```
salaam → salaam      bazaar → bazaar      aardvark → aardvark
Isaac → Isaac        Canaan → Canaan      graal → graal
```

Và tiếng Việt không bị ảnh hưởng, vì `caa` chưa tạo ra mũ nào để mà huỷ:

```
caa → câ      caan → cân      khuaay → khuây      aa → â
```

!!! tip "Gõ cả đoạn tiếng Anh dài?"

    Đừng chiến đấu với từng từ — nhấn ++ctrl+shift+z++ để tạm tắt tiếng Việt.
    Xem [ToggleVietnameseKey](../guide/configuration.md#togglevietnamesekey).

## VNI không có vấn đề này

Ở [chế độ VNI](../guide/telex-vni.md#vni), phím phụ là **chữ số**, nên chữ cái không bao giờ bị
hiểu sai. `person`, `address`, `cheese` luôn nguyên vẹn mà không cần kiểm tra chính tả gì cả.

Đây là lý do đáng cân nhắc VNI nếu bạn gõ lẫn tiếng Anh rất nhiều.

## Liên quan

- [Âm tiết tiếng Việt](vietnamese-syllable.md) — bảng âm đầu/vần dùng để kiểm tra hợp lệ
- [Cách gõ Telex & VNI](../guide/telex-vni.md) — bảng phím phụ đầy đủ
- [Tuỳ chọn cấu hình](../guide/configuration.md) — bật/tắt SpellCheckRestore
