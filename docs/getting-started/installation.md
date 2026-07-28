# Cài đặt

Có 5 cách cài. Nếu bạn dùng Ubuntu/Debian, dùng cách đầu tiên.

| Cách | Phù hợp khi | Tự cập nhật |
|---|---|:---:|
| [APT repo](#apt-repo) | Ubuntu/Debian — **khuyên dùng** | ✅ |
| [Gói `.deb`](#deb) | Ubuntu/Debian, tải tay | ❌ |
| [Gói `.rpm`](#rpm) | Fedora/CentOS | ❌ |
| [CMake thủ công](#cmake) | Distro khác, hoặc muốn sửa code | ❌ |
| [`install.sh`](#install-sh) | Build nhanh từ source | ❌ |

---

## Cách 1 — APT repo (Ubuntu/Debian) { #apt-repo }

Thêm repo một lần, sau đó `apt upgrade` như mọi gói khác.

```bash
# 1. Thêm khoá ký của repo
curl -fsSL https://sonnam0904.github.io/telebit/pubkey.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/telebit-archive-keyring.gpg

# 2. Thêm repo (jammy = Ubuntu 22.04, noble = 24.04)
echo "deb [signed-by=/usr/share/keyrings/telebit-archive-keyring.gpg] https://sonnam0904.github.io/telebit $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/telebit.list

# 3. Cài
sudo apt update
sudo apt install telebit
```

`telebit` là metapackage, chỉ phụ thuộc gói thật **`telebit-fcitx5`** — nên
`sudo apt install telebit-fcitx5` cũng cho kết quả y hệt.

!!! info "Repo hiện hỗ trợ jammy và noble"

    Nếu `lsb_release -cs` trả về codename khác (ví dụ `bookworm` của Debian), repo sẽ không có
    suite tương ứng. Khi đó dùng [`.deb` tải tay](#deb) hoặc [CMake](#cmake).

Xong thì sang [Dùng lần đầu](first-use.md).

---

## Cách 2 — Gói `.deb` tải tay { #deb }

File `.deb` **không nằm trong repo source**; mỗi bản được build trên GitHub Actions.

**1. Tải**

| Nguồn | Cách lấy |
|---|---|
| **Releases** | [Trang Releases](https://github.com/sonnam0904/telebit/releases) — mỗi tag có hai `.deb`, hậu tố `+jammy` (22.04) hoặc `+noble` (24.04) |
| **Artifacts** | **Actions** → **Release** → run mới nhất → **Artifacts** → `telebit-fcitx5-deb-jammy` / `-noble` |

**2. Cài**

```bash
cd ~/Downloads
sudo apt update
sudo apt install -y ./telebit-fcitx5_*_amd64.deb
```

!!! warning "Bắt buộc có `./`"

    Không có `./` (hoặc đường dẫn tuyệt đối), `apt` sẽ hiểu đó là **tên gói trên mirror** và
    báo không tìm thấy.

Nếu dùng `dpkg` và thiếu dependency:

```bash
sudo dpkg -i ./telebit-fcitx5_*_amd64.deb
sudo apt-get install -f -y
```

**3. Khởi động lại fcitx5**

```bash
fcitx5 -r
```

---

## Cách 3 — Gói `.rpm` (Fedora/CentOS) { #rpm }

Gói yêu cầu `fcitx5` đã có sẵn (nằm trong repo mặc định của Fedora).

Tải `.rpm` từ [Releases](https://github.com/sonnam0904/telebit/releases) hoặc Actions → Artifacts
(tên dạng `telebit-fcitx5-*-fedora43.rpm`), rồi:

```bash
cd ~/Downloads
sudo dnf install -y ./telebit-fcitx5*.rpm
fcitx5 -r
```

---

## Cách 4 — CMake thủ công { #cmake }

**Cài dependency trước:**

=== "Ubuntu/Debian"

    ```bash
    sudo apt update
    sudo apt install -y \
      fcitx5 fcitx5-configtool fcitx5-config-qt fcitx5-module-lua \
      libfcitx5core-dev libfcitx5utils-dev libcurl4-openssl-dev \
      extra-cmake-modules cmake build-essential
    ```

=== "Fedora"

    ```bash
    sudo dnf install -y fcitx5 fcitx5-configtool fcitx5-devel libcurl-devel \
      gcc-c++ cmake make extra-cmake-modules
    ```

=== "Arch"

    ```bash
    sudo pacman -S --needed base-devel cmake fcitx5 fcitx5-configtool curl \
      extra-cmake-modules
    ```

**Build và cài:**

=== "Toàn hệ thống (khuyên dùng)"

    ```bash
    cd telebit-fcitx5
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr .
    cmake --build build
    sudo cmake --install build
    fcitx5 -r
    ```

=== "Chỉ cho user hiện tại"

    ```bash
    cd telebit-fcitx5
    cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local" .
    cmake --build build
    cmake --install build
    fcitx5 -r
    ```

!!! danger "Đừng chọn `--user` nếu fcitx5 cài qua APT"

    fcitx5 chỉ nạp addon `.so` từ thư mục lib addon **của chính bản build nó** — thường là
    `/usr/lib/<arch>/fcitx5`. Nó **không** tự dò `$HOME/.local/lib/fcitx5`.

    Nếu fcitx5 của bạn đến từ APT/`.deb`/`.rpm` (trường hợp phổ biến nhất), phải cài
    **toàn hệ thống**, nếu không addon sẽ không bao giờ xuất hiện. Chỉ dùng user-local khi bạn
    tự build và chạy fcitx5 từ `$HOME/.local`.

---

## Cách 5 — Script `install.sh` { #install-sh }

Ở thư mục gốc repository:

```bash
./install.sh            # mặc định = --system (PREFIX=/usr, cần sudo)
./install.sh --system
./install.sh --user     # PREFIX=$HOME/.local
```

Script sẽ: build core C++ → **chạy bộ test** → build addon → `cmake --install` theo PREFIX.
Test fail là script dừng, không cài gì cả.

---

## Sau khi cài

Chưa xong đâu — cần bật fcitx5 và thêm input method vào danh sách:
**[Dùng lần đầu](first-use.md)**.

!!! tip "Chưa thấy `telebit-fcitx5` trong danh sách?"

    Lần cài đầu tiên thường cần **logout/login** hoặc reboot, vì fcitx5 quét danh sách addon
    khi khởi động session. Nếu vẫn không thấy, xem
    [Xử lý sự cố](../reference/troubleshooting.md).
