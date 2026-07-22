<a id="readme-top"></a>

<div align="center">
  <h2 align="center">Telebit</h2>

  <p align="center">
    <b>Vietnamese Telex engine — C++ core và addon fcitx5 cho Linux</b>
    <br />
    <a href="https://github.com/sonnam0904/telebit"><strong>Xem repository »</strong></a>
    <br />
    <br />
    <a href="https://github.com/sonnam0904/telebit/releases">
      <img src="https://img.shields.io/github/v/release/sonnam0904/telebit?style=flat&color=success" alt="Release">
    </a>
    <a href="https://github.com/sonnam0904/telebit/blob/main/README.md">
      <img src="https://img.shields.io/badge/docs-README-informational?style=flat" alt="Documentation">
    </a>
    <a href="https://github.com/sonnam0904/telebit/stargazers">
      <img src="https://img.shields.io/github/stars/sonnam0904/telebit?style=flat&color=yellow" alt="Stars">
    </a>
    <a href="https://github.com/sonnam0904/telebit/network/members">
      <img src="https://img.shields.io/github/forks/sonnam0904/telebit?style=flat&color=orange" alt="Forks">
    </a>
    <a href="https://github.com/sonnam0904/telebit/issues">
      <img src="https://img.shields.io/github/issues/sonnam0904/telebit?style=flat&color=red" alt="Issues">
    </a>
  </p>

  <p align="center">
    <a href="#installation"><strong>Hướng dẫn cài đặt »</strong></a>
    &nbsp;&middot;&nbsp;
    <a href="remove-vnkey.md"><strong>Gỡ vnkey cũ</strong></a>
    &nbsp;&middot;&nbsp;
    <a href="vietnamese.md"><strong>Vietnamese syllable structure</strong></a>
    <br />
    <br />
    <a href="https://github.com/sonnam0904/telebit/issues/new">Báo cáo lỗi</a>
  </p>
</div>

<br />


`Telebit` là một bộ gõ Telex tiếng Việt gồm:

- **Core C++ engine**: hàm `telex_to_unicode` chuyển chuỗi Telex ASCII → Unicode tiếng Việt (dùng được độc lập).
- **fcitx5 addon**: module `telebit-fcitx5` cho Linux desktop (GNOME/KDE/…).

Thiết kế dựa trên **cấu trúc âm tiết tiếng Việt** (*Âm đầu + Vần + Thanh*). Mô tả chi tiết xem thêm trong [`vietnamese.md`](vietnamese.md).

---

## 1. Cấu trúc thư mục

Trong thư mục gốc của repository:

- **`vietnamese.h/.cpp`**: API `telex_to_unicode(const std::string&)` – chuyển một chuỗi Telex thành Unicode.
- **`engine.h/.cpp`**: lớp `EngineVietCpp` – quản lý buffer gõ theo từng phím, dùng trong fcitx5.
- **`rime_table.*`**: bảng vần + vị trí “nguyên âm chính” để đặt dấu.
- **`canonicalize.*`**: pipeline canonicalize input (escape rules, tách âm đầu/vần, chuẩn hoá vị trí w/aa/ee/oo, trích xuất thanh…).
- **`render_utf8.*`**: render nội bộ → Unicode UTF‑8 (dấu, chữ hoa, v.v.).
- **`telebit-fcitx5/`**: mã nguồn addon cho fcitx5.
- **`install.sh`**: script cài đặt nhanh (build core + addon fcitx5).

---

## 2. Build core C++ và chạy test

**Yêu cầu:**

- Compiler hỗ trợ **C++17**.
- **CMake ≥ 3.10**.

**Cài công cụ build (gợi ý):**

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake

# Fedora
sudo dnf install -y gcc-c++ cmake make

# Arch
sudo pacman -S --needed base-devel cmake
```

**Build + chạy test** (binary: `telebit_telex_tests`):

```bash
cmake -B build .
cmake --build build
./build/telebit_telex_tests
```

Nếu mọi thứ ổn, chương trình sẽ in:

```text
All C++ tests passed.
```

**Build sạch khi đổi nhánh/pull code lớn:**

```bash
rm -rf build
cmake -B build .
cmake --build build
./build/telebit_telex_tests
```

Nếu trước đó bạn dùng bộ gõ **vnkey** cũ và muốn chuyển sang Telebit: xem hướng dẫn chi tiết trong [`remove-vnkey.md`](remove-vnkey.md).

---

<a id="installation"></a>

## 3. Cài addon `telebit-fcitx5` cho fcitx5

<a id="install-menu"></a>

> **Menu chọn cách cài đặt (có 4 cách — bấm để nhảy nhanh):**
>
> - **APT repo (khuyên dùng cho Ubuntu/Debian)**: [`sudo apt install telebit`](#install-apt-repo) — cài 1 lần, tự nhận bản cập nhật sau này qua `apt upgrade`.
> - **CMake (thủ công)**: [`/usr` (system-wide)](#install-cmake-system) · [`$HOME/.local` (user-local)](#install-cmake-user)
> - **Gói cài sẵn (tải tay)**: [`.deb` (Ubuntu/Debian)](#install-deb) · [`.rpm` (Fedora/CentOS)](#install-rpm)
> - **Cài nhanh**: [`install.sh`](#install-script)

### 3.0. Cách dễ nhất: APT repo (Ubuntu / Debian)

<a id="install-apt-repo"></a>

Thêm repo một lần, sau đó `apt install`/`apt upgrade` như mọi gói khác — không cần tải file `.deb` thủ công mỗi lần có bản mới:

```bash
# 1. Thêm khoá ký của repo
curl -fsSL https://sonnam0904.github.io/telebit/pubkey.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/telebit-archive-keyring.gpg

# 2. Thêm repo (codename Ubuntu của bạn: 22.04 = jammy, 24.04 = noble)
echo "deb [signed-by=/usr/share/keyrings/telebit-archive-keyring.gpg] https://sonnam0904.github.io/telebit $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/telebit.list

# 3. Cài
sudo apt update
sudo apt install telebit
```

`telebit` là metapackage tiện dùng, chỉ phụ thuộc vào gói thật **`telebit-fcitx5`** (nên `apt install telebit-fcitx5` cũng chạy y hệt). Sau khi cài, làm tiếp **mục 4** để bật fcitx5 và thêm input method.

### 3.1. Yêu cầu

- Đã cài **fcitx5** và header phát triển (tuỳ distro: `libfcitx5core-dev`, `fcitx5-devel`, …).

**Gợi ý cài đặt (Linux):**

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
  fcitx5 fcitx5-configtool fcitx5-config-qt fcitx5-module-lua \
  libfcitx5core-dev libfcitx5utils-dev \
  extra-cmake-modules cmake build-essential

# Fedora (tên gói có thể thay đổi theo phiên bản)
sudo dnf install -y fcitx5 fcitx5-configtool fcitx5-devel

# Arch
sudo pacman -S --needed fcitx5 fcitx5-configtool
```

### 3.2. Cài đặt thủ công bằng CMake

- **Cài toàn hệ thống** (đề xuất, cần `sudo`):

<a id="install-cmake-system"></a>

```bash
cd telebit-fcitx5
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr .
cmake --build build
sudo cmake --install build
```

- **Cài cho user hiện tại** (không dùng `sudo`, cài vào `$HOME/.local`):

<a id="install-cmake-user"></a>

```bash
cd telebit-fcitx5
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local" .
cmake --build build
cmake --install build
```

Sau khi cài, restart fcitx5:

```bash
fcitx5 -r
```

Nếu mới cài lần đầu mà chưa thấy `telebit-fcitx5` trong danh sách input method, hãy **logout/login** hoặc **reboot** máy.

### 3.3. Cài bằng gói `.deb` (Ubuntu / Debian và dòng Debian)

<a id="install-deb"></a>

File `.deb` **không nằm trong repo**; mỗi bản được build trên GitHub Actions (hoặc bạn tự chạy `scripts/build-deb.sh` ở máy local). Gói **`telebit-fcitx5`** chứa addon fcitx5; trường `Depends` kéo **fcitx5** và gói `libfcitx5core*` phù hợp.

**1. Tải `.deb`**

| Nguồn | Cách lấy |
|--------|-----------|
| **Releases** | [Releases](https://github.com/sonnam0904/telebit/releases) → mỗi tag thường có **hai** `.deb`: tên gói có hậu tố **`+jammy`** (22.04) hoặc **`+noble`** (24.04) — chọn đúng bản cho máy. |
| **Artifacts** | Mỗi push `main` có hai ZIP: **`telebit-fcitx5-deb-jammy`** và **`telebit-fcitx5-deb-noble`**. **Actions** → **Release** → run mới nhất → **Artifacts** (giải nén ra `.deb`). |

Tên file dạng `telebit-fcitx5_<phiên-bản>+jammy_amd64.deb` hoặc `…+noble_…`.

**2. Cài (Ubuntu / Debian — dùng `apt`)**

Vào thư mục chứa file vừa tải (hoặc chỉ đường dẫn đầy đủ). **Phải có `./`** (hoặc đường dẫn tuyệt đối) để `apt` hiểu là file local, không phải tên gói trên mirror:

```bash
cd ~/Downloads   # ví dụ
sudo apt update
sudo apt install -y ./telebit-fcitx5_*_amd64.deb
```

Nếu bạn dùng `dpkg` và gặp lỗi thiếu dependency:

```bash
sudo dpkg -i ./telebit-fcitx5_*_amd64.deb
sudo apt-get install -f -y
```

**3. Sau khi cài**

```bash
fcitx5 -r
```

Rồi thêm input method **Telebit** (tìm `telebit-fcitx5`) trong `fcitx5-configtool` như **mục 4**. Lần đầu có thể cần **đăng xuất / khởi động lại** session nếu chưa thấy `telebit-fcitx5` trong danh sách.

**Gỡ:** nếu đã cài bằng `.deb`, dùng **mục 6.1** (`sudo apt remove telebit-fcitx5`).

### 3.4. Cài bằng gói `.rpm` (Fedora / CentOS)

<a id="install-rpm"></a>

File `.rpm` cũng được thiết kế tự động build trên GitHub Actions mỗi khi có bản release mới. Gói này yêu cầu hệ thống phải cài đặt sẵn `fcitx5` (có trong repository chính mặc định của Fedora).

**1. Tải `.rpm`**
Tương tự file DEB, bạn có thể lấy gói `.rpm` từ tab [Releases](https://github.com/sonnam0904/telebit/releases) hoặc nhánh **Actions** -> Artifacts. Tên file thường có dạng `telebit-fcitx5-*-fedora43.rpm`.

**2. Cài đặt (bằng `dnf`)**
Vào thư mục chứa file vừa tải về và sử dụng `dnf` cài đặt:

```bash
cd ~/Downloads
sudo dnf install -y ./telebit-fcitx5*.rpm
```

**3. Sau khi cài**
Khởi động lại tiến trình engine:

```bash
fcitx5 -r
```

Và sau đó thiết lập thêm input method **Telebit** (tìm `telebit-fcitx5`) trong `fcitx5-configtool` như mục 4 ở dưới.

### 3.5. Cài nhanh bằng script `install.sh`

<a id="install-script"></a>

Ở thư mục gốc của repository:

```bash
# Cài toàn hệ thống (PREFIX = /usr, cần sudo) — mặc định nếu không truyền cờ
./install.sh
./install.sh --system

# Cài cho user hiện tại (PREFIX = $HOME/.local)
./install.sh --user
```

> Lưu ý: fcitx5 chỉ tìm thư viện addon (`.so`) trong thư mục lib addon đã compile sẵn của chính nó (thường là `/usr/lib/<arch>/fcitx5` với bản cài qua `apt`) — **không** tự động dò thêm `$HOME/.local/lib/fcitx5`. Nếu fcitx5 của bạn cài qua APT/`.deb`/`.rpm` (phổ biến nhất), dùng `--system` (mặc định) để addon được nhận diện ngay; chỉ dùng `--user` khi bạn tự build và chạy fcitx5 từ `$HOME/.local`.

Script sẽ thực hiện:

- Build core C++ và chạy `./build/telebit_telex_tests`.
- Build addon **telebit-fcitx5** (thư mục `telebit-fcitx5/`).
- Cài đặt với `cmake --install` theo `PREFIX` tương ứng.

---

## 4. Bật fcitx5 và chọn input method `telebit-fcitx5`

### 4.1. Bật fcitx5 làm input method mặc định (nếu chưa cấu hình)

Nếu bạn chưa dùng fcitx5, addon `telebit-fcitx5` sẽ cài thành công nhưng **không hoạt động** do ứng dụng vẫn dùng input method khác (IBus, etc.).

- **Bước 1** – Cài fcitx5 (xem mục 3) và mở GUI cấu hình:

```bash
fcitx5-configtool
```

- **Bước 2** – Đặt fcitx5 làm input method mặc định cho session:

- **Cách A (Ubuntu/Debian)**:

```bash
im-config -n fcitx5
```

- **Cách B (GNOME/KDE, cấu hình tay)**: thêm các biến môi trường vào nơi phù hợp (`~/.profile`, `~/.xprofile`, hoặc file env của desktop):

```bash
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
```

- **Bước 3** – Logout/login (hoặc reboot), sau đó đảm bảo fcitx5 đang chạy:

```bash
fcitx5 -d
```

### 4.2. Thêm `telebit-fcitx5` vào danh sách input method

1. Mở `fcitx5-configtool`.
2. Vào tab **Input Method** → bấm **Add** → tìm `telebit-fcitx5`.  
   Tên hiển thị: **“Vietnamese Telex (UTF-8) - Telebit (telebit-fcitx5)”**.
3. Thêm vào danh sách, bấm **Apply**.
4. Dùng phím tắt của fcitx5 để chuyển sang `telebit-fcitx5`, và thử:
   - `tieengs vieetj` → `tiếng việt`
   - `nguyeenx` → `nguyễn`

---

## 5. Chế độ gõ trong `telebit-fcitx5`

Addon `telebit-fcitx5` hỗ trợ **2 chế độ chính**:

### 5.1. Chế độ mặc định: preedit (gạch chân)

- Ký tự đang gõ hiển thị dưới dạng **preedit có gạch chân**.
- Khi nhấn Space / Enter / dấu câu, từ sẽ được “chốt” và gửi vào ứng dụng.
- Ưu điểm:
  - Ổn định.
  - Tương thích tốt với hầu hết ứng dụng (editor, terminal, trình duyệt, v.v.).

### 5.2. Chế độ direct commit + rollback (không preedit)

- Bật/tắt trong `fcitx5-configtool`:
  - Tab **Addons** → chọn **telebit-fcitx5** → nút **Configure** →
  - Tích/ bỏ **DirectCommitRollback** (mặc định đang bật).
- Hiệu ứng:
  - Chữ Việt xuất hiện **trực tiếp** trong ô nhập (không có gạch chân).
  - Ví dụ: gõ `nguyeenx` sẽ thấy dần biến thành `nguyễn` ngay trong ứng dụng.
- Lưu ý:
  - Một số ứng dụng không hỗ trợ đầy đủ tính năng của fcitx5, khi đó `telebit-fcitx5` có thể tự động quay về chế độ preedit để tránh lỗi.
  - Hành vi Undo/Redo trong một số ứng dụng có thể khác đôi chút so với chế độ mặc định. Nếu cảm thấy khó chịu, hãy tắt **DirectCommitRollback**.

### 5.3. Các tuỳ chọn khác (Addons → telebit-fcitx5 → Configure)

| Tuỳ chọn | Mặc định | Ý nghĩa |
|---|---|---|
| **SpellCheckRestore** | Bật | Kiểm tra chính tả: nếu từ vừa gõ không phải âm tiết tiếng Việt hợp lệ thì tự khôi phục về phím gốc (giúp gõ xen tiếng Anh: `person`, `address`, `cheese`… không bị biến dạng). |
| **VNIMode** | Tắt | Chuyển sang kiểu gõ **VNI**: `1-5` = sắc/huyền/hỏi/ngã/nặng, `0` = xoá thanh, `6` = mũ (â/ê/ô), `7` = móc (ư/ơ), `8` = ă, `9` = đ. Ví dụ: `vie65t` → `việt`, `d9i` → `đi`. Gõ đúp số để ra số literal (`a11` → `a1`); đuôi số như `nam2024` được giữ nguyên. |
| **ModernToneStyle** | Tắt | Đặt dấu kiểu mới cho vần `oa/oe/uy`: `hoá, khoẻ, thuý` (mặc định là kiểu cũ: `hóa, khỏe, thúy`). |
| **AutoCapitalizeSentence** | Bật | Tự động viết hoa chữ cái đầu tiên của câu tiếp theo sau khi gõ `.`, `?`, `!` rồi nhấn dấu cách hoặc Enter. |
| **ToggleVietnameseKey** | `Ctrl+Shift+Z` | Tạm bật/tắt gõ tiếng Việt (hữu ích khi cần gõ một đoạn tiếng Anh dài). |
| **Macros** | (trống) | Gõ tắt: khai báo cặp *viết tắt → nội dung* (vd: `vn` → `Việt Nam`). Khi kết thúc từ (Space/Enter/dấu câu), từ viết tắt được thay bằng nội dung. |

---

## Cấu hình trợ lý AI

Telebit có **trợ lý AI**: mở ô nhập ngay trong lúc gõ (`Ctrl+Shift+Space`), viết yêu cầu — kèm ngữ cảnh copy được (`Ctrl+V`) nếu muốn — rồi `Enter` để AI sinh văn bản và chèn thẳng vào ứng dụng. Hữu ích cho: soạn/trả lời tin nhắn, dịch, tóm tắt, sửa câu…

Mọi tham số (API key, endpoint, model, temperature…) đều đọc từ **biến môi trường**, và bạn có thể tạo sẵn các **skill** (`/tên-skill`) để tái sử dụng hướng dẫn.

📖 **Xem hướng dẫn đầy đủ: [Trợ lý AI](docs/ai-assistant.md)** — bật tính năng, bảng biến môi trường, cách dùng phím tắt, và tạo skill.

---

## 6. Gỡ cài đặt addon `telebit-fcitx5`

### 6.1. Nếu đã cài bằng gói `.deb` hoặc qua APT repo

```bash
sudo apt remove telebit telebit-fcitx5
fcitx5 -r
```

Nếu cài qua APT repo (mục 3.0) và muốn gỡ luôn cả repo:

```bash
sudo rm -f /etc/apt/sources.list.d/telebit.list /usr/share/keyrings/telebit-archive-keyring.gpg
sudo apt update
```

### 6.2. Nếu đã cài vào `/usr` (system-wide)

```bash
sudo rm -f /usr/lib/fcitx5/telebit-fcitx5.so
sudo rm -f /usr/share/fcitx5/addon/telebit-fcitx5.conf
sudo rm -f /usr/share/fcitx5/inputmethod/telebit-fcitx5.conf
fcitx5 -r
```

### 6.3. Nếu đã cài vào `$HOME/.local` (user-local)

```bash
rm -f "$HOME/.local/lib/fcitx5/telebit-fcitx5.so"
rm -f "$HOME/.local/share/fcitx5/addon/telebit-fcitx5.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/telebit-fcitx5.conf"
fcitx5 -r
```

---

## ✨ Star History

<a href="https://star-history.com/#sonnam0904/telebit&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&legend=top-left" />
 </picture>
</a>
