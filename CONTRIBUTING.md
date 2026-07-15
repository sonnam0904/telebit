# Hướng dẫn đóng góp cho Telebit

Cảm ơn bạn đã quan tâm đóng góp cho **Telebit** — bộ gõ Telex tiếng Việt (C++
core + fcitx5 addon cho Linux). Mọi đóng góp đều được hoan nghênh: báo lỗi, đề
xuất tính năng, cải thiện tài liệu, hay gửi mã nguồn.

## Bắt đầu

1. **Fork** repository và clone về máy.
2. Tạo nhánh mới từ `main`:
   ```bash
   git checkout -b feat/ten-tinh-nang
   ```
3. Cài đặt phụ thuộc build (xem chi tiết trong [README](README.md)):
   - CMake, trình biên dịch C++ (g++/clang++)
   - fcitx5 và các gói phát triển tương ứng (khi build addon)

## Build & Test

Dự án dùng CMake. Trong thư mục gốc:

```bash
cmake -S . -B build
cmake --build build
```

Trước khi gửi Pull Request, hãy chạy bộ test và đảm bảo tất cả đều pass:

```bash
# Test được định nghĩa trong tests.cpp
./build/<binary-test>
```

Nếu bạn thêm tính năng mới hoặc sửa lỗi liên quan tới logic chuyển đổi Telex →
Unicode, **hãy bổ sung test tương ứng** trong [`tests.cpp`](tests.cpp).

## Quy ước commit

Dự án sử dụng [Conventional Commits](https://www.conventionalcommits.org/) kết
hợp với [semantic-release](https://github.com/semantic-release/semantic-release)
để tự động tạo phiên bản và CHANGELOG. Vui lòng đặt commit message theo định
dạng:

| Prefix     | Ý nghĩa                                | Ảnh hưởng version |
|------------|----------------------------------------|-------------------|
| `feat:`    | Tính năng mới                          | minor             |
| `fix:`     | Sửa lỗi                                | patch             |
| `docs:`    | Chỉ thay đổi tài liệu                   | không             |
| `refactor:`| Tái cấu trúc, không đổi hành vi         | không             |
| `test:`    | Thêm/sửa test                          | không             |
| `chore:`   | Việc lặt vặt (build, CI, deps…)         | không             |

Breaking change: thêm `!` sau prefix (ví dụ `feat!:`) hoặc footer
`BREAKING CHANGE:` để tăng major version.

Ví dụ:

```
feat: thêm hỗ trợ vần "uây"
fix: đặt dấu sai với nguyên âm đôi "ươ"
```

## Gửi Pull Request

1. Đảm bảo code build thành công và test pass.
2. Cập nhật tài liệu (`README.md`, `vietnamese.md`) nếu cần.
3. Mở Pull Request tới nhánh `main` với mô tả rõ ràng về thay đổi.
4. Điền đầy đủ mẫu Pull Request được cung cấp.

## Báo lỗi & Đề xuất

- Dùng mẫu **Bug report** để báo lỗi, kèm các bước tái hiện.
- Dùng mẫu **Feature request** để đề xuất tính năng.

Xem thêm [Quy tắc ứng xử](CODE_OF_CONDUCT.md) trước khi tham gia.

Cảm ơn bạn! 🎉
