# Kiểm tra bằng từ điển trước khi mở PR

`telebit_telex_tests` chỉ kiểm những âm tiết đã có người nghĩ ra và viết vào file test. Nó
không nói được rằng thay đổi của bạn vừa làm hỏng 20 từ khác mà chưa ai nghĩ tới.

`scripts/dict-roundtrip/run.sh` làm việc đó: chạy **toàn bộ từ điển** qua engine và **gọi tên
từng từ** mà thay đổi của bạn làm hỏng, làm đúng lại, hoặc làm ra kết quả khác trước.

!!! warning "Bước này không nằm trong CI"

```
CI chỉ chạy `telebit_telex_tests` và test CLI. Kiểm tra từ điển là việc bạn phải tự chạy
trước khi mở PR — và đính kèm baseline mới nếu có thay đổi.
```

## Vì sao cần

Nó chạy hai chiều ngược nhau, và hai chiều này kéo nhau:


| Chiều                       | Kiểm gì                                                                     |
| --------------------------- | --------------------------------------------------------------------------- |
| **Tiếng Việt** (round-trip) | Âm tiết → chuỗi phím Telex → convert lại → phải ra đúng âm tiết ban đầu     |
| **Tiếng Anh** (passthrough) | Từ tiếng Anh gõ trong chế độ tiếng Việt → phải ra y hệt, không byte nào đổi |


Nới bảng vần làm nhiều chuỗi phím trở nên convert được, tức là ăn vào tiếng Anh. Chỉ chạy một
chiều thì bạn chỉ thấy một nửa hệ quả.

## Chuẩn bị

Chỉ cần thư mục `build/` đã configure (xem [Build & test](build-and-test.md)):

```bash
cmake -B build .
```

Từ điển đã nằm sẵn trong repo ở `dict/`, **không cần cài gì và không cần mạng**:


| File                                 | Nguồn                              | Giấy phép |
| ------------------------------------ | ---------------------------------- | --------- |
| `dict/vietnamese` — 6.631 âm tiết    | package `hunspell-vi` `1:24.2.1-1` | GPL-2     |
| `dict/american-english` — 104.334 từ | package `wamerican` `2020.12.07-2` | kiểu BSD  |


!!! note "Vì sao commit từ điển vào repo thay vì tải lúc chạy"

```
Baseline là bản ghi **từng-từ-một**. Nếu corpus khác nhau giữa các máy thì phiên bản từ
điển của người khác sẽ hiện ra như thể là hồi quy do bạn gây ra. Ghim từ điển vào repo loại
bỏ hẳn nhóm vấn đề đó, và cho phép chạy offline.

Script cố tình **không** tự đoán sang bản ở `/usr/share/dict`: thiếu file là dừng với exit
3 kèm thông báo, vì chạy âm thầm trên một corpus khác còn tệ hơn là không chạy.
```

Telebit là MIT còn từ điển tiếng Việt là GPL-2 — hai bộ này chỉ là **dữ liệu kiểm thử**, đọc
lúc chạy test, không link vào binary và không nằm trong `.deb`/`.rpm`. Chi tiết giấy phép và
cách dựng lại khi nâng phiên bản: `dict/README.md`.

## Quy trình



### 1. Chạy trên nhánh sạch trước

Trước khi sửa gì, chạy một lần để chắc chắn baseline khớp với máy bạn:

```bash
./scripts/dict-roundtrip/run.sh
```

```text
  So với baseline
┌───┬────────────┬──────────┬─────────────┬────────┬───────────────┐
│   │ Ngôn ngữ   │ Mới hỏng │ Đổi kết quả │ Đã sửa │ Kết luận      │
├───┼────────────┼──────────┼─────────────┼────────┼───────────────┤
│ ✔ │ Tiếng Việt │        0 │           0 │      0 │ khớp baseline │
│ ✔ │ Tiếng Anh  │        0 │           0 │      0 │ khớp baseline │
└───┴────────────┴──────────┴─────────────┴────────┴───────────────┘

  ✔ Không có thay đổi nào so với baseline.
```

Vì từ điển nằm trong repo nên bước này gần như luôn sạch. Nếu **chưa sửa gì** mà đã thấy khác
biệt thì nhánh của bạn đang có thay đổi trong `dict/` hoặc `baseline/` — kiểm tra `git status`
trước, đừng chạy tiếp.

### 2. Sửa code, rồi chạy lại

```bash
./scripts/dict-roundtrip/run.sh
```

Script tự build lại core nên không cần `cmake --build` thủ công.

### 3. Đọc kết quả

Ví dụ thật, khi vần `uôm` bị xoá và vần `uynh` bị trả về cách đặt dấu cũ:

```text
  So với baseline
┌───┬────────────┬──────────┬─────────────┬────────┬───────────────┐
│   │ Ngôn ngữ   │ Mới hỏng │ Đổi kết quả │ Đã sửa │ Kết luận      │
├───┼────────────┼──────────┼─────────────┼────────┼───────────────┤
│ ! │ Tiếng Việt │       20 │           0 │      0 │ CÓ HỒI QUY    │
│ ✔ │ Tiếng Anh  │        0 │           0 │      0 │ khớp baseline │
└───┴────────────┴──────────┴─────────────┴────────┴───────────────┘

  ! Tiếng Việt · Mới hỏng (20 từ)
┌────────┬──────────────┬───────────────┐
│ Từ     │ Gõ bằng phím │ Engine cho ra │
├────────┼──────────────┼───────────────┤
│ buồm   │ buoomf       │ buoomf        │
│ huỳnh  │ huynhf       │ hùynh         │
│ nhuộm  │ nhuoomj      │ nhuoomj       │
│ quỳnh  │ quynhf       │ qùynh         │
└────────┴──────────────┴───────────────┘

  ! CÓ HỒI QUY. Hãy sửa, hoặc nếu đây là đánh đổi có chủ ý thì giải thích
     trong mô tả PR rồi chạy: ./scripts/dict-roundtrip/run.sh --update-baseline
```

So hai cột cuối là biết ngay hỏng kiểu nào:


| Hai cột cuối                         | Nghĩa                                                                                                                                    |
| ------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Giống hệt nhau (`buoomf` → `buoomf`) | Engine từ chối convert, trả lại phím thô. Thường là **thiếu vần** trong `rime_table.cpp`. Người dùng thấy chữ Latin nên nhận ra ngay.    |
| Khác nhau (`huynhf` → `hùynh`)       | Engine có convert nhưng **sai** — đặt dấu nhầm chỗ hoặc dựng nguyên âm sai. Người dùng có thể không nhận ra, nên loại này nguy hiểm hơn. |


Bảng tiếng Anh chỉ có hai cột, vì từ tiếng Anh chính là chuỗi phím. Mỗi nhóm in tối đa 25
dòng; nếu nhiều hơn, script nói rõ còn bao nhiêu và file nào trong `.work/` chứa đầy đủ.

Phía trên hai bảng này còn hai bảng nữa — **Ngữ liệu** (kích thước từng bộ từ điển) và **Kết
quả thô** (số đạt/lỗi từng phép kiểm). Chỉ dòng *hỏng ở CẢ HAI kiểu* mới là lỗi thật; hai dòng
kiểu cũ / kiểu mới lệch nhau chỉ vì từ điển trộn lẫn hai quy ước bỏ dấu.

## Ba nhãn và cách xử lý


|     | Nhãn            | Nghĩa                                       | Exit  | Bạn phải làm gì                                                       |
| --- | --------------- | ------------------------------------------- | ----- | --------------------------------------------------------------------- |
| `!` | **Mới hỏng**    | Giờ hỏng, baseline không có                 | **1** | Điều tra. Hoặc sửa, hoặc giải thích rõ trong PR vì sao chấp nhận được |
| `!` | **Đổi kết quả** | Vẫn hỏng ở cả hai bên nhưng ra kết quả khác | **1** | Xem kỹ — hành vi đã đổi dù tổng số không đổi                          |
| `✔` | **Đã sửa được** | Baseline có, giờ đã hết hỏng                | 0     | Chạy `--update-baseline` để ghi nhận                                  |


**Đổi kết quả** là thứ mà cách đếm số không bao giờ bắt được. Một thay đổi sửa ba âm tiết
nhưng làm hỏng ba âm tiết khác sẽ giữ nguyên con số tổng — chỉ có so **danh sách từ** mới thấy.

```text
  ! Tiếng Việt · Đổi kết quả (1 từ)
┌───────┬──────────────┬──────────────────────────┐
│ Từ    │ Gõ bằng phím │ Engine cho ra            │
├───────┼──────────────┼──────────────────────────┤
│ boong │ boong        │ bông   (baseline: bôông) │
└───────┴──────────────┴──────────────────────────┘
```



### Khi *Mới hỏng* là đánh đổi có chủ ý

Không phải từ *Mới hỏng* nào cũng là bug. Nếu bạn mở rộng bảng vần để cứu từ tiếng Việt và mất vài từ
tiếng Anh, đó có thể là đánh đổi đúng. Khi đó trong mô tả PR hãy nêu rõ:

- Cứu được bao nhiêu, mất bao nhiêu (script in sẵn con số)
- Liệt kê các từ *Mới hỏng* và vì sao chúng chấp nhận được
- Vì sao không có cách nào tránh được

Đừng im lặng cập nhật baseline rồi coi như không có gì.

## Cập nhật baseline và đưa vào PR

Đây là bước **bắt buộc** khi kết quả có thay đổi và bạn đã xác nhận là đúng ý:

```bash
./scripts/dict-roundtrip/run.sh --update-baseline
```

```text
  ✔ Đã chốt baseline mới: 16 âm tiết Việt, 1203 từ Anh
    Nhớ commit scripts/dict-roundtrip/baseline cùng trong PR.
```

Rồi commit **cùng trong PR đó**:

```bash
git add scripts/dict-roundtrip/baseline/
git commit -m "test: cập nhật baseline từ điển"
```

!!! danger "Không commit baseline là làm hỏng cổng kiểm tra cho người sau"

```
Baseline là mốc để so sánh. Nếu bạn sửa code làm thay đổi kết quả mà không cập nhật
baseline, mọi người sau bạn đều thấy `FAIL` do thay đổi **của bạn** chứ không phải của họ —
và cổng này lập tức mất tác dụng vì ai cũng bỏ qua nó.

Ngược lại, nếu bạn chỉ có *Đã sửa được* mà không cập nhật baseline, cổng sẽ tiếp tục bảo vệ trạng
thái cũ và tệ hơn, tức là bug bạn vừa sửa có thể quay lại mà không ai biết.
```

Hai file baseline nằm ở `scripts/dict-roundtrip/baseline/`:


| File             | Nội dung                                                            |
| ---------------- | ------------------------------------------------------------------- |
| `vietnamese.tsv` | 16 dòng — đọc bằng mắt được, mỗi dòng là `âm tiết · phím · kết quả` |
| `english.tsv`    | 1203 dòng — mỗi dòng là `từ · kết quả`                              |


Diff của hai file này trong PR chính là bằng chứng rõ nhất cho reviewer thấy thay đổi của bạn
ảnh hưởng tới đâu.

!!! tip "Nếu bạn nâng phiên bản từ điển trong `dict/`"

```
Đổi corpus gần như chắc chắn làm baseline lệch một loạt từ. Khi đó `git add` cả `dict/` lẫn
`baseline/`, và **nói rõ trong mô tả PR rằng đây là thay đổi từ điển chứ không phải thay
đổi engine** — nếu không reviewer sẽ tưởng bộ gõ vừa hỏng thêm hàng chục từ.
```



## Checklist trước khi mở PR

```bash
cmake --build build && ./build/telebit_telex_tests    # test đơn vị
./scripts/dict-roundtrip/run.sh                       # kiểm tra từ điển
```

- [ ] `telebit_telex_tests` pass
- [ ] `run.sh` không còn *Mới hỏng* / *Đổi kết quả* nào chưa giải thích được
- [ ] Đã thêm assert vào `tests.cpp` cho từ mà bạn vừa sửa được — script chỉ chặn hồi quy, còn
  ```
  test đơn vị mới ghi lại **ý định**
  ```
- [ ] Đã chạy `--update-baseline` và commit `scripts/dict-roundtrip/baseline/` nếu kết quả đổi
- [ ] Mô tả PR nêu rõ đánh đổi, nếu có



## Giới hạn cần biết

Script này kiểm **core chuyển đổi** (`telex_to_unicode`), không kiểm tầng engine hay preedit
của fcitx5. Lỗi ở `engine.cpp` hay `telebit_fcitx5.cpp` — backspace, commit, rollback — nó
không thấy.

Nó cũng chỉ dùng `hunspell-vi` (~6.6k âm tiết đã biên tập) chứ không dùng các wordlist lớn thu
thập tự động, vì những bộ đó trộn lẫn biến thể phương ngữ, từ láy chép sai và tên riêng nên
"âm tiết này hỏng" không phân biệt được với "mục này vốn không phải âm tiết tiếng Việt". Chi
tiết ở `scripts/dict-roundtrip/README.md`.

Và vì từ điển được ghim ở một phiên bản cố định, nó chỉ phủ đúng những từ có trong hai file đó
— từ mới, tên riêng hay cách viết vùng miền nằm ngoài `dict/` thì nó không biết tới.

Cuối cùng, **16 lỗi tiếng Việt và 1203 lỗi tiếng Anh trong baseline không phải bug cần sửa** —
chúng là mơ hồ cố hữu của Telex (`xoong` đụng `xông` vì `oo` là `ô`; `reset` → `rết` vì `s` là
phím dấu sắc). Đừng cố kéo chúng về 0.

## Liên quan

- [Build & test](build-and-test.md) — build core và viết test đơn vị
- [Kiểm tra chính tả & escape](../concepts/spell-check.md) — cơ chế quyết định một âm tiết có
được convert hay bị trả về phím thô
- [Âm tiết tiếng Việt](../concepts/vietnamese-syllable.md) — cấu trúc onset + vần mà bảng vần
dựa vào

