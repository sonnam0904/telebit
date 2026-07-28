# Cách gõ Telex & VNI

Telebit hỗ trợ hai kiểu gõ. Mặc định là **Telex**; bật **VNIMode** trong
[Tuỳ chọn cấu hình](configuration.md) để chuyển sang VNI.

## Telex

### Thanh điệu

| Phím | Thanh | Ví dụ |
|---|---|---|
| `s` | sắc | `as` → á |
| `f` | huyền | `af` → à |
| `r` | hỏi | `ar` → ả |
| `x` | ngã | `ax` → ã |
| `j` | nặng | `aj` → ạ |

### Nguyên âm có dấu phụ

| Gõ | Ra | Gõ | Ra |
|---|---|---|---|
| `aa` | â | `ow` | ơ |
| `aw` | ă | `uw` | ư |
| `ee` | ê | `uow` | ươ |
| `oo` | ô | `dd` | đ |

Ghép lại được: `aas` → ấ, `eej` → ệ, `uws` → ứ, `owf` → ờ.

### Phím phụ đặt ở đâu cũng được

Không cần gõ phím phụ ngay sau nguyên âm — đặt cuối từ cũng được:

| Gõ | Ra |
|---|---|
| `tieengs` | tiếng |
| `vieetj` | việt |
| `nguyeenx` | nguyễn |
| `quyeets` | quyết |
| `hoanfg` | hoàng |

### Vị trí dấu tự động

Engine biết đặt dấu vào **nguyên âm chính** theo cấu trúc âm tiết, không phải cứ nguyên âm đầu:

| Gõ | Ra | Dấu nằm ở |
|---|---|---|
| `thuys` | thúy | `u` |
| `hoas` | hóa | `o` |
| `ruowuj` | rượu | `ơ` |
| `khuyur` | khuỷu | `y` |
| `ngoeof` | ngoèo | `e` |

Đổi quy ước `oa/oe/uy` bằng [ModernToneStyle](configuration.md#modern-tone).

Chi tiết lý thuyết: [Âm tiết tiếng Việt](../concepts/vietnamese-syllable.md).

### Gõ thoát — khi bạn muốn ký tự nguyên bản

Gõ **lặp phím phụ** để lấy lại chữ gốc:

| Gõ | Ra | Vì sao |
|---|---|---|
| `ss` `ff` `rr` `xx` `jj` | `s` `f` `r` `x` `j` | Huỷ thanh vừa đặt |
| `aaa` | `aa` | Huỷ mũ |
| `eee` | `ee` | `leeech` → leech |
| `ooo` | `oo` | `cooool` → coool |
| `ddd` | `dd` | `edddy` → eddy |
| `ww` | `w` | `sunwworld` → sunworld |

Xem thêm [Kiểm tra chính tả & escape](../concepts/spell-check.md) — phần lớn thời gian bạn
**không cần** gõ thoát, vì engine tự nhận ra từ tiếng Anh.

---

## VNI { #vni }

Bật **VNIMode**. Phím phụ là **chữ số**, nên chữ cái không bao giờ bị hiểu là phím phụ — gõ xen
tiếng Anh an toàn tuyệt đối.

### Thanh điệu

| Phím | Thanh | Ví dụ |
|---|---|---|
| `1` | sắc | `ba1` → bá |
| `2` | huyền | `cha2o` → chào |
| `3` | hỏi | `ba3` → bả |
| `4` | ngã | `ba4` → bã |
| `5` | nặng | `ban5` → bạn |
| `0` | xoá thanh | `toa10n` → toan |

### Dấu phụ

| Phím | Ra | Ví dụ |
|---|---|---|
| `6` | â / ê / ô | `vie65t` → việt, `toi6` → tôi |
| `7` | ơ / ư | `tu7o7ng` → tương |
| `8` | ă | `a8n` → ăn |
| `9` | đ | `d9i` → đi, `di9` → đi |

Đặt ở đâu cũng được, như Telex: `vie65t` và `viet65` đều ra **việt**.

### Số literal

| Gõ | Ra | Vì sao |
|---|---|---|
| `a11` | `a1` | Gõ đúp số để lấy số thật |
| `nam2024` | `nam2024` | Đuôi số được giữ nguyên |
| `ba10` | `ba10` | Cặp số ở cuối là số, không phải phím phụ |
| `2024` | `2024` | Số mở đầu không vào buffer |

---

## Giữ nguyên chữ hoa

Cả hai kiểu gõ đều bảo toàn cách viết hoa:

| Gõ | Ra |
|---|---|
| `Vieetj` | Việt |
| `VIEETJ NAM` | VIỆT NAM |
| `Nguyeenx` | Nguyễn |
| `DDI` | ĐI |

## Liên quan

- [Kiểm tra chính tả & escape](../concepts/spell-check.md) — vì sao `cheese` không thành `chêse`
- [Âm tiết tiếng Việt](../concepts/vietnamese-syllable.md) — bảng vần và quy tắc đặt dấu
- [Tuỳ chọn cấu hình](configuration.md) — bật VNI, đổi kiểu đặt dấu
