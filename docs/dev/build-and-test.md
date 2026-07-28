# Build & test

Core C++ build được **không cần fcitx5** — hữu ích khi bạn chỉ muốn làm việc với bộ chuyển đổi.

## Yêu cầu

- Compiler hỗ trợ **C++17**
- **CMake ≥ 3.10**

=== "Ubuntu/Debian"

    ```bash
    sudo apt update
    sudo apt install -y build-essential cmake
    ```

=== "Fedora"

    ```bash
    sudo dnf install -y gcc-c++ cmake make
    ```

=== "Arch"

    ```bash
    sudo pacman -S --needed base-devel cmake
    ```

Build addon fcitx5 thì cần thêm header của fcitx5 và libcurl — xem
[Cài đặt → CMake](../getting-started/installation.md#cmake).

## Build core và chạy test

```bash
cmake -B build .
cmake --build build
./build/telebit_telex_tests
```

Ổn thì in ra:

```text
All C++ tests passed.
```

Build sạch khi đổi nhánh hoặc pull thay đổi lớn:

```bash
rm -rf build
cmake -B build .
cmake --build build
./build/telebit_telex_tests
```

!!! note "Mặc định là Release"

    `CMakeLists.txt` tự đặt `CMAKE_BUILD_TYPE=Release` nếu bạn không truyền gì, vì các đường cài
    (`install.sh`, `cmake -B build .`) đều không truyền — mà thiếu nó thì binary không có tối ưu
    nào.

    Bộ test vẫn kiểm tra thật ở mọi build type: `tests.cpp` chủ động `#undef NDEBUG` trước khi
    include `<cassert>`, nên `assert` không bị compile bỏ.

## Viết test

Test nằm gọn trong `tests.cpp`, mỗi nhóm là một hàm `static void test_*()` gọi từ `main()`.
Cách kiểm tra là `assert` trên `telex_to_unicode()`:

```cpp
static void test_my_feature() {
    assert(telex_to_unicode("tieengs") == "tiếng");
    assert(telex_to_unicode("cheese") == "cheese");
}
```

Thêm hàm mới thì nhớ gọi nó trong `main()`, nếu không nó không bao giờ chạy.

Có sẵn helper cho các cấu hình khác: `vni_opts()`, `no_spell_check_opts()`, `modern_tone_opts()`.

!!! tip "Test cho hành vi tiếng Anh nữa"

    Mỗi thay đổi trong pipeline chuyển đổi đều có nguy cơ làm hỏng phần giữ nguyên tiếng Anh.
    Khi thêm rule mới, thêm luôn cả ca **không nên** bị chuyển — xem
    `test_spell_check_restore()` và `test_trailing_hat_escape()` làm mẫu.

## CI

Workflow `.github/workflows/ci.yml` build core và chạy test trên mọi push và pull request.
`release.yml` gọi lại đúng workflow đó và `needs: test`, nên test fail thì **không** ra package
và **không** publish release.

## Cài nhanh cả addon

```bash
./install.sh            # = --system (PREFIX=/usr)
./install.sh --user     # PREFIX=$HOME/.local
```

Script build core → **chạy test** → build addon → `cmake --install`. Test fail là dừng, không cài
gì cả.

## Đóng gói

| Script | Ra |
|---|---|
| `scripts/build-deb.sh` | `.deb` (dùng `TELEBIT_DEB_PACKAGE_SUFFIX` để đặt jammy/noble) |
| `scripts/build-rpm.sh` | `.rpm` |
| `scripts/build-meta-deb.sh` | metapackage `telebit` |
| `scripts/apt-repo-publish.sh` | thêm `.deb` vào APT repo tĩnh rồi ký lại index |

## Liên quan

- [Kiến trúc](architecture.md) — luồng chuyển đổi và vai trò từng file
- [Kiểm tra chính tả & escape](../concepts/spell-check.md) — phần dễ hỏng nhất khi sửa
