# Cấu thành tiếng Việt (âm tiết)

Tài liệu này tóm lược cách cấu thành một tiếng (âm tiết) tiếng Việt dựa trên bộ sách Tiếng Việt lớp 1 và dùng làm nền tảng cho bộ gõ/engine xử lý theo mô hình âm tiết.

## Cấu trúc chuẩn của một âm tiết

Một âm tiết tiếng Việt thường được viết thành một “chữ” và có cấu trúc:

**Âm tiết = Âm đầu + Vần + Thanh**

- **Âm đầu**: phụ âm đứng đầu (có thể rỗng).
- **Vần**: phần còn lại của tiếng (thường bắt đầu bằng nguyên âm).
- **Thanh**: dấu thanh (6 loại, gồm cả ngang).

## Cách ghép thành tiếng (2 bước)

- **Bước 1**: Ghép **âm đầu + vần** để tạo ra tiếng cơ sở.
- **Bước 2**: Thêm **dấu thanh** vào vị trí đúng trong vần.

Ví dụ:

| Âm đầu | Vần | Tiếng |
|---|---|---|
| b | a | ba |
| m | e | me |
| l | oa | loa |
| th | uy | thuy |
| tr | ang | trang |

## 6 thanh trong tiếng Việt

| Thanh | Ví dụ |
|---|---|
| ngang | ba |
| sắc | bá |
| huyền | bà |
| hỏi | bả |
| ngã | bã |
| nặng | bạ |

Ví dụ ghép đầy đủ:

| Âm đầu | Vần | Thanh | Tiếng |
|---|---|---|---|
| b | a | ngang | ba |
| b | a | sắc | bá |
| b | a | huyền | bà |

## Nhóm phụ âm đầu (âm đầu)

Các nhóm âm đầu:

- **c – k**
- **g – gh**
- **ng – ngh**
- **q – r**
- **s – x**
- **t – th**
- **tr – ch**
- **n – nh**
- **kh – m**
- **p – ph**
- **v – y**

Ghi chú: trong thực tế, khi phân tích âm đầu, thường dùng nguyên tắc **khớp dài nhất trước** (ví dụ `ngh` trước `ng`).

## Nhóm vần theo âm cuối

### Nhóm vần có âm cuối **m / p**

- **am – ap**
- **ăm – ăp**
- **âm – ấp**
- **em – ep**
- **êm – êp**
- **im – ip**
- **iêm – iếp**
- **om – op**
- **ôm – ốp**
- **ơm – ớp**
- **um – up**
- **ươm – ướp**

### Nhóm vần có âm cuối **n / t**

- **an – at**
- **ăn – ăt**
- **ân – ất**
- **en – et**
- **ên – êt**
- **in – it**
- **iên – iết**
- **on – ot**
- **ôn – ốt**
- **ơn – ớt**
- **un – ut**
- **uôn – uốt**
- **ươn – ướt**

### Nhóm vần có âm cuối **ng / c**

- **ang – ac**
- **ăng – ăc**
- **âng – âc**
- **eng – ec**
- **iêng – iếc**
- **ong – oc**
- **ông – ôc**
- **ung – uc**
- **ưng – ưc**
- **uông – uốc**
- **ương – ước**

### Nhóm vần có âm cuối **nh / ch**

- **anh – ach**
- **ênh – êch**
- **inh – ich**
- **oanh – oach**
- **uynh – uych**

## Nhóm vần nguyên âm ghép

- **ai – ay**
- **ao – eo**
- **au – âu**
- **eo – êu**
- **êu – yêu**
- **oi – ôi**
- **ôi – ơi**
- **ui – ưi**
- **uôi – ươi**
- **oa – oe**
- **uê – uơ**
- **ua – uô**
- **uy – uya**
- **iu – ưu** (`cứu`, `lưu`, `mưu`, `hưu`)
- **ươu** (`rượu`, `hươu`, `bươu`, `khướu`)

## Nhóm vần mở rộng

- **oan – oat**
- **oăn – oăt**
- **uân – uất**
- **oen – oet**
- **oang – oac**
- **uyên – uyết**
- **uyn – uyt**

## Nhóm vần ba nguyên âm (mở, không âm cuối)

Các vần gồm ba nguyên âm và không có âm cuối. Dấu thanh đặt trên **nguyên âm giữa** (xem quy tắc bên dưới).

- **oeo** – dấu trên `e` (`ngoèo`, `ngoẻo`)
- **uyu** – dấu trên `y` (`khuỷu`)
- **uây** – dấu trên `â` (`khuây`, `khuẩy`)

## Quy tắc đặt dấu (thanh) trong vần

Khi thêm **thanh** vào một tiếng, dấu thanh được đặt lên **nguyên âm chính** của vần.

- Với **vần 1 nguyên âm**: dấu đặt trên nguyên âm đó.
- Với **vần nhiều nguyên âm**: cần xác định **nguyên âm chính** (tùy nhóm vần/chuẩn chính tả), rồi đặt dấu lên nguyên âm chính.

Ví dụ (minh hoạ thường gặp):

- **ngoại**: vần `oai`, dấu nặng đặt trên **a** → `ngoại`
- **xoáy**: vần `oay`, dấu sắc đặt trên **a** → `xoáy`
- **cứu**: vần `ưu`, dấu sắc đặt trên **ư** → `cứu`
- **rượu**: vần `ươu`, dấu nặng đặt trên **ơ** → `rượu`
- **khuỷu**: vần `uyu`, dấu hỏi đặt trên **y** → `khuỷu`
- **ngoèo**: vần `oeo`, dấu huyền đặt trên **e** → `ngoèo`
- **khuẩy**: vần `uây`, dấu hỏi đặt trên **â** → `khuẩy`

