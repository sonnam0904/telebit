# Kiểm tra bộ gõ bằng từ điển

`tests.cpp` chỉ kiểm những âm tiết có người nghĩ ra và viết vào. Bộ harness này
kiểm cả từ điển, theo hai chiều ngược nhau — nhờ nó mới tìm ra các vần thiếu
`uôm`/`uênh`/`uêch`, lỗi đặt dấu vần `uynh`, và việc onset `gi` nuốt mất dấu.

**Tiếng Việt — round-trip.** Với mỗi âm tiết: **Unicode → chuỗi phím Telex →
`telex_to_unicode()` → so với gốc**. `to_telex.py` lo phần ánh xạ ngược (tách
NFD từng ký tự, đổi `â`/`ê`/`ô` thành nguyên âm đôi, `ă`/`ơ`/`ư` thành nguyên âm
+ `w`, `đ` thành `dd`, và dời phím dấu về cuối âm tiết), nên chuỗi phím sinh ra
đúng là thứ người dùng thật sự gõ.

**Tiếng Anh — passthrough.** Mỗi từ phải trả về y hệt từng byte. Hai phép đo này
**kéo ngược nhau**: nới bảng vần làm nhiều chuỗi phím trở nên convert được, và
bắt đầu ăn vào từ tiếng Anh. Chính việc thêm `oam`/`oap`/`oem` để cứu `ngoạm` và
`ngoém` đã biến `roams` thành `roám`. Chạy cả hai, không thì chỉ thấy một nửa hệ
quả của thay đổi.

## Cách chạy

```sh
./run.sh                     # so với baseline đã commit
./run.sh --update-baseline   # chốt trạng thái hiện tại làm baseline mới
```

Không có gì đổi:

```
  So với baseline
┌───┬────────────┬──────────┬─────────────┬────────┬───────────────┐
│   │ Ngôn ngữ   │ Mới hỏng │ Đổi kết quả │ Đã sửa │ Kết luận      │
├───┼────────────┼──────────┼─────────────┼────────┼───────────────┤
│ ✔ │ Tiếng Việt │        0 │           0 │      0 │ khớp baseline │
│ ✔ │ Tiếng Anh  │        0 │           0 │      0 │ khớp baseline │
└───┴────────────┴──────────┴─────────────┴────────┴───────────────┘

  ✔ Không có thay đổi nào so với baseline.
```

Sau khi sửa code, nó gọi tên từng từ:

```
  ! Tiếng Việt · Mới hỏng (20 từ)
┌────────┬──────────────┬───────────────┐
│ Từ     │ Gõ bằng phím │ Engine cho ra │
├────────┼──────────────┼───────────────┤
│ buồm   │ buoomf       │ buoomf        │
│ huỳnh  │ huynhf       │ hùynh         │
│ nhuộm  │ nhuoomj      │ nhuoomj       │
└────────┴──────────────┴───────────────┘
```

Cột **Gõ bằng phím** và **Engine cho ra** giống nhau nghĩa là engine từ chối
convert; khác nhau nghĩa là nó convert nhưng ra sai. Bảng tiếng Anh chỉ có hai
cột vì từ tiếng Anh chính là chuỗi phím.

Mỗi nhóm in tối đa 25 dòng; phần còn lại script nói rõ còn bao nhiêu và nằm ở
file nào trong `.work/`.

File trung gian nằm trong `.work/` (đã git-ignore); baseline nằm trong
`baseline/` và được commit.

## Ba loại thay đổi mà diff bắt được

| | Nhãn | Nghĩa | Exit |
|---|---|---|---|
| `!` | **Mới hỏng** | Giờ hỏng, baseline không có | 1 |
| `!` | **Đổi kết quả** | Vẫn hỏng ở cả hai bên, nhưng engine cho ra kết quả khác | 1 |
| `✔` | **Đã sửa được** | Baseline có, giờ đã hết hỏng | 0 |

So **danh sách từ** thay vì so **số lượng** chính là điểm mấu chốt: một thay đổi
sửa được ba âm tiết nhưng làm hỏng ba âm tiết khác sẽ giữ nguyên con số tổng.
**Đổi kết quả** lo trường hợp thứ ba, khi một âm tiết vẫn hỏng nhưng hỏng *theo
kiểu khác* — `boong` đang ra `bông` mà chuyển thành thứ khác là một thay đổi hành
vi đáng thấy, dù tổng số không nhúc nhích.

**Đã sửa được** không làm fail, vì cải thiện thì không nên chặn ai. Nhưng nó có nghĩa là
baseline đã cũ: chạy `--update-baseline` rồi commit, nếu không cái cổng này sẽ
tiếp tục bảo vệ trạng thái cũ và tệ hơn.

`--update-baseline` luôn ghi cả hai baseline cùng lúc. Không có tình huống chỉ
ghi một nửa: từ điển nằm trong repo nên hoặc chạy được cả hai chiều, hoặc script
dừng ngay từ đầu với exit 3.

## Nguồn từ điển

Cả hai bộ nằm trong `dict/` ở gốc repo và **được commit**, nên `run.sh` không cần
mạng, không cần package hệ thống, và cho ra cùng con số trên mọi máy. Thiếu file
là script dừng với exit 3 chứ không tự đoán sang bản của hệ thống — một corpus
khác âm thầm còn tệ hơn là không chạy.

| File | Nguồn | Giấy phép |
|---|---|---|
| `dict/vietnamese` | package `hunspell-vi` `1:24.2.1-1` | GPL-2 |
| `dict/american-english` | package `wamerican` `2020.12.07-2` | kiểu BSD |

Cách dựng lại khi cần nâng phiên bản: xem `dict/README.md`.

### Vì sao chỉ dùng hunspell-vi

Vì đây là danh sách âm tiết **đã được biên tập**, nên một lỗi trong đó có ý
nghĩa. Các wordlist lớn thu thập tự động — Viet74K và tương tự — không dùng làm
căn cứ được: chúng trộn lẫn biến thể phương ngữ, từ láy chép sai và tên riêng,
nên "âm tiết này hỏng" không phân biệt được với "mục này vốn chưa bao giờ là âm
tiết tiếng Việt".

Chạy với Viet74K cho ra 439 lỗi và một danh sách vần cần thêm — `uêu`, `oây`,
`uăng` — mà **không vần nào tồn tại**. Chúng đến từ `khuều khoào`, `ngoe ngoẩy`
và `khua khuắng`; các dạng viết đúng (`khều`, `nguẩy`, `khuấy`) thì engine đã xử
lý đúng từ trước. Cùng corpus đó, chỉ dùng hunspell-vi: 16 lỗi, và cả 16 đều
giải thích được.

## Đọc các con số thô

Bên tiếng Việt chạy cả hai kiểu bỏ dấu, và một âm tiết chỉ bị tính là lỗi khi
hỏng ở **cả hai**. Bản thân hunspell-vi không nhất quán — nó viết `hoà` (kiểu
mới) nhưng `hủy` (kiểu cũ) — nên chạy một kiểu thôi sẽ báo hàng chục lỗi giả (85
ở kiểu cũ, 16 ở kiểu mới).

Lỗi có hai dạng, và dạng thứ hai mới là dạng đáng truy:

| Dạng | Nghĩa |
|---|---|
| kết quả == chuỗi phím | Cổng spell-check từ chối âm tiết nên trả lại phím thô. Thường là thiếu vần trong `rime_table.cpp`. Người dùng thấy chữ Latin nên nhận ra ngay. |
| kết quả == một chữ khác | Engine có convert, nhưng sai — đặt dấu nhầm chỗ hoặc dựng nguyên âm sai. Người dùng có thể không nhận ra. |

### 16 âm tiết trong baseline tiếng Việt

Không cái nào là lỗi cần sửa; đây là các ngoại lệ cố định, liệt kê ra để sau này
baseline có thay đổi thì dễ giải thích.

| Số | Âm tiết | Vì sao |
|---:|---|---|
| 13 | `boong` `boóng` `choòng` `coong` `coóc` `goòng` `loong` `moóc` `soong` `soóc` `toong` `toòng` `xoong` | `oo` là `ô` trong Telex, nên các từ mượn này đụng `bông`/`cống`/… Gõ `xooong` để ra chữ literal. |
| 2 | `GIF` `HĐND` | Từ viết tắt. `GIF` → `GÌ` là cố ý (`tests.cpp` có assert); `HĐND` cần `dd` → `Đ` ở giữa từ viết hoa. |
| 1 | `palăng` | Hai âm tiết viết liền; harness không tách được. |

Baseline tiếng Anh cũng giữ hai lớp không thể triệt tiêu của riêng nó: những từ
mà chữ cái tình cờ đánh vần thành âm tiết Việt thật (`reset`→`rết`,
`karma`→`kẩm`), và những từ có phụ âm đôi nằm sau một tiền tố dạng âm tiết
(`chess`→`ches`, `off`→`of`) — chuỗi phím của chúng giống hệt thao tác gõ escape
có chủ ý. Danh sách này để **theo dõi biến động**, không phải để kéo về 0.
