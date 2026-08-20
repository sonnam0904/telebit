# Từ điển dùng cho kiểm thử

Hai bộ từ điển mà `scripts/dict-roundtrip/run.sh` chạy qua engine. Chúng được
**commit thẳng vào repo** chứ không tải lúc chạy, vì baseline của bộ kiểm tra là
bản ghi **từng-từ-một**: nếu corpus khác nhau giữa các máy thì phiên bản từ điển
của người khác sẽ hiện ra như thể là hồi quy do bạn gây ra.

Đổi lại, `run.sh` chạy được offline, không cần cài package hệ thống, và cho ra
đúng cùng con số trên mọi máy lẫn CI.

| File | Dòng | Nội dung |
|---|---:|---|
| `vietnamese` | 6.631 | Âm tiết tiếng Việt, mỗi dòng một âm tiết |
| `american-english` | 104.334 | Từ tiếng Anh, mỗi dòng một từ |

## Nguồn và phiên bản

| | Nguồn | Phiên bản | Giấy phép |
|---|---|---|---|
| `vietnamese` | package `hunspell-vi` (LibreOffice dictionaries) | `1:24.2.1-1` | **GPL-2** |
| `american-english` | package `wamerican` (SCOWL, Kevin Atkinson) | `2020.12.07-2` | kiểu BSD |

Toàn văn giấy phép nằm ở `vietnamese.LICENSE` và `american-english.LICENSE`.

> **Lưu ý về giấy phép.** Bản thân telebit là **MIT** (xem `../LICENSE`), còn bộ
> từ điển tiếng Việt là **GPL-2**. Hai bộ này chỉ là dữ liệu kiểm thử: chúng được
> đọc lúc chạy test, không bao giờ được link vào hay đóng gói kèm binary — `.deb`
> và `.rpm` không chứa thư mục này. Code của telebit vì thế vẫn thuần MIT, nhưng
> **bản phân phối repo** thì có kèm dữ liệu GPL-2.

## Cách dựng lại

Nếu cần cập nhật lên phiên bản từ điển mới hơn:

```bash
cd /tmp && mkdir -p dictsrc && cd dictsrc
apt-get download hunspell-vi wamerican      # không cần cài, chỉ tải .deb
dpkg-deb -x hunspell-vi_*.deb vi
dpkg-deb -x wamerican_*.deb   en

# Tiếng Việt: bỏ dòng đếm ở đầu file, bỏ cờ hunspell sau dấu "/", lọc trùng
tail -n +2 vi/usr/share/hunspell/vi_VN.dic | sed 's:/.*::' | sort -u \
    > "${REPO}/dict/vietnamese"

# Tiếng Anh: chép nguyên vẹn
cp en/usr/share/dict/american-english "${REPO}/dict/american-english"
```

Không thêm, bớt hay sửa từ nào ngoài các bước trên — nếu cần lọc gì thêm thì lọc
trong `run.sh`, để `dict/` luôn đối chiếu được với bản upstream.

Nhớ cập nhật cả phiên bản trong bảng phía trên và trong hai file `.LICENSE`.

## Sau khi đổi từ điển

Đổi corpus gần như chắc chắn làm baseline lệch. Chạy lại và commit baseline mới
**cùng trong PR đó**:

```bash
./scripts/dict-roundtrip/run.sh                    # xem thay đổi những gì
./scripts/dict-roundtrip/run.sh --update-baseline
git add dict/ scripts/dict-roundtrip/baseline/
```

Trong mô tả PR nói rõ đây là thay đổi **từ điển**, không phải thay đổi **engine**
— nếu không reviewer sẽ tưởng bộ gõ vừa hỏng thêm một loạt từ.

## Vì sao không dùng wordlist lớn hơn

Xem phần *Vì sao chỉ dùng hunspell-vi* trong
`../scripts/dict-roundtrip/README.md`. Tóm tắt: các bộ thu thập tự động trộn lẫn
biến thể phương ngữ, từ láy chép sai và tên riêng, nên "âm tiết này hỏng" không
phân biệt được với "mục này vốn không phải âm tiết tiếng Việt" — và điều đó đã
từng dẫn tới một danh sách vần cần thêm mà không vần nào tồn tại thật.
