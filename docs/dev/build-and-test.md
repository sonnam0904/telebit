# Build & test

Core C++ build được **không cần fcitx5** — hữu ích khi bạn chỉ muốn làm việc với bộ chuyển đổi.

## Yêu cầu

Toàn repo dùng chung một mức tối thiểu: compiler **C++17** và **CMake ≥ 3.21**.

Repo có **hai** project CMake tách rời, khác nhau ở dependency chứ không ở CMake:

| Project | Cần thêm |
|---|---|
| Core (`./`) — engine + `telebit_telex_tests` | không gì cả |
| `telebit-fcitx5/` — addon + CLI `telebit` | header fcitx5, libcurl, extra-cmake-modules |

Phần dưới đây cài dependency cho **core**:

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



## Build và test CLI `telebit doctor`

CLI nằm trong project `telebit-fcitx5/`, không phải project core, nên build riêng:

```bash
cd telebit-fcitx5
cmake -B build .
cmake --build build
./build/cli/telebit_doctor_tests
```

!!! warning "Bước này cần dependency của addon, dù CLI không dùng tới"

    `cli/CMakeLists.txt` cố tình không link gì từ fcitx5 — nhưng `cmake -B build .` ở đây
    configure **cả project**, gồm cả target addon, nên vẫn đòi `Fcitx5Core`, `CURL` và
    `extra-cmake-modules`. Chỉ cài `build-essential cmake` như phần core là configure sẽ
    fail. Cài đủ theo [Cài đặt → CMake](../getting-started/installation.md#cmake) trước.

Build xong có luôn binary chạy được tại `build/cli/telebit` (target tên `telebit_cli`
nhưng `OUTPUT_NAME` là `telebit`), tiện để thử mà không cần cài đè lên máy:

```bash
./build/cli/telebit doctor
./build/cli/telebit doctor --markdown
```

Hoặc qua ctest:

```bash
ctest --test-dir build --output-on-failure
```



### Vì sao code CLI chia làm ba tầng


| File              | Việc                                                  | Test được?                                    |
| ----------------- | ----------------------------------------------------- | --------------------------------------------- |
| `cli/probe.cpp`   | Đọc hệ thống thật: `/proc`, `busctl`, thư mục runtime | Không — không có gì tất định để khẳng định    |
| `cli/verdict.cpp` | Biến dữ kiện thành phán quyết                         | **Có** — hàm thuần, nhận struct trả về struct |
| `cli/doctor.cpp`  | Vẽ bảng ra terminal                                   | Chỉ phần đo chữ (`cli/textfmt.cpp`)           |


Ranh giới này không phải cho đẹp. Nó là **cách duy nhất** để test các nhánh Wayland từ một máy
X11: test dựng thẳng `SessionInfo{display_server="wayland", compositor="kwin"}` rồi gọi
`judge_session`, không cần compositor nào đang chạy.

Cũng vì thế mà thay đổi cách hiển thị không bao giờ làm đổi kết luận của báo cáo — hai thứ nằm
ở hai file khác nhau.

### Viết thêm test

Mỗi ca trong `cli/tests.cpp` đánh dấu `regression:` là một bug **đã từng lọt ra thật**. Ví dụ
`pad_to` từng thêm 1 space thừa khi chuỗi dài đúng bằng cột, và khoảng trống module từng bị
đánh dấu `LỖI` khiến mã thoát báo máy hỏng trong khi mọi ứng dụng vẫn gõ bình thường.

Kiểm tra test có thật sự bắt lỗi bằng cách cố tình phá lại rồi build:

```bash
sed -i 's|column - width : 0|column - width : 1|' cli/textfmt.cpp   # gây lại bug cũ
cmake --build build && ./build/cli/telebit_doctor_tests             # phải FAIL
```



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

Workflow `.github/workflows/ci.yml` chạy trên mọi push và pull request, gồm **hai job**:

| Job | Làm gì | Vì sao tách |
|---|---|---|
| `test` | Build core, chạy `telebit_telex_tests` | Chỉ cần compiler + cmake có sẵn trên runner |
| `test-cli` | Build `telebit-fcitx5/`, chạy `ctest` (suite `doctor`) | Phải `apt-get install` header fcitx5 + libcurl trước |

`release.yml` gọi lại đúng workflow này và các job `package-deb` / `package-rpm` / `release`
đều `needs: test`. Workflow được gọi chỉ thành công khi **mọi** job trong nó thành công, nên
test core *hoặc* test CLI fail đều chặn không ra package và không publish release.

## Cài nhanh cả addon

```bash
./install.sh            # = --system (PREFIX=/usr)
./install.sh --user     # PREFIX=$HOME/.local
```

Script chạy tuần tự: build core → **chạy test core** → build addon + CLI → **chạy test CLI**
(`build/cli/telebit_doctor_tests`) → `cmake --install`. Script đặt `set -euo pipefail` nên bất
kỳ bước test nào fail là dừng ngay, không cài gì cả — kể cả khi core đã pass mà tầng verdict
của doctor hỏng.

## Đóng gói


| Script                        | Ra                                                            |
| ----------------------------- | ------------------------------------------------------------- |
| `scripts/build-deb.sh`        | `.deb` (dùng `TELEBIT_DEB_PACKAGE_SUFFIX` để đặt jammy/noble/resolute/bookworm/trixie) |
| `scripts/build-rpm.sh`        | `.rpm` (dùng `TELEBIT_RPM_PACKAGE_SUFFIX` để đặt fedora43/fedora44) |
| `scripts/build-meta-deb.sh`   | metapackage `telebit`                                         |
| `scripts/apt-repo-publish.sh` | thêm `.deb` vào APT repo tĩnh rồi ký lại index                |

Gói ra chứa cả addon `.so` **và** binary `/usr/bin/telebit` — cùng một build, nên chúng không
bao giờ lệch phiên bản.

Biến môi trường quyết định chuỗi phiên bản, `telebit-fcitx5/CMakeLists.txt` đọc theo thứ tự ưu
tiên: `TELEBIT_PACKAGE_VERSION` → `TELEBIT_DEB_PACKAGE_VERSION` → `PROJECT_VERSION` (`0.1.0`).
Chuỗi chốt được ở đây chính là thứ `telebit --version` in ra và là giá trị trường `Version` của
addon, nên hai bên không thể mâu thuẫn.




## Liên quan

- [Kiến trúc](architecture.md) — luồng chuyển đổi và vai trò từng file
- [Kiểm tra chính tả & escape](../concepts/spell-check.md) — phần dễ hỏng nhất khi sửa

