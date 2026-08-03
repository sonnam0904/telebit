<a id="readme-top"></a>

<div align="center">

<img src="telebit-fcitx5/icons/telebit-fcitx5.svg" alt="Telebit" width="88" height="88">

# Telebit

**Bộ gõ Telex / VNI tiếng Việt cho Linux**

Engine C++ dùng độc lập được + addon `telebit-fcitx5` cho fcitx5 trên GNOME / KDE / …

<p>
  <a href="https://github.com/sonnam0904/telebit/releases">
    <img src="https://img.shields.io/github/v/release/sonnam0904/telebit?style=flat-square&color=success&label=phiên%20bản" alt="Release">
  </a>
  <a href="https://sonnam0904.github.io/telebit/">
    <img src="https://img.shields.io/badge/tài%20liệu-website-informational?style=flat-square" alt="Documentation">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/github/license/sonnam0904/telebit?style=flat-square&color=blue" alt="License">
  </a>
  <a href="https://github.com/sonnam0904/telebit/stargazers">
    <img src="https://img.shields.io/github/stars/sonnam0904/telebit?style=flat-square&color=yellow" alt="Stars">
  </a>
  <a href="https://github.com/sonnam0904/telebit/issues">
    <img src="https://img.shields.io/github/issues/sonnam0904/telebit?style=flat-square&color=red" alt="Issues">
  </a>
</p>

<p>
  <a href="#-cài-đặt"><b>⬇️ Cài đặt</b></a>
  &nbsp;·&nbsp;
  <a href="https://sonnam0904.github.io/telebit/"><b>📖 Tài liệu đầy đủ</b></a>
  &nbsp;·&nbsp;
  <a href="#-tuỳ-chọn-cấu-hình"><b>⚙️ Cấu hình</b></a>
  &nbsp;·&nbsp;
  <a href="#-xử-lý-sự-cố"><b>🩺 Sự cố</b></a>
  &nbsp;·&nbsp;
  <a href="https://github.com/sonnam0904/telebit/issues/new"><b>🐞 Báo lỗi</b></a>
</p>

</div>

---

## Telebit gõ như thế nào?

| Bạn gõ | Kết quả |
|---|---|
| `tieengs vieetj` | **tiếng việt** |
| `nguyeenx` | **nguyễn** |
| `Vieetj Nam 2024` | **Việt Nam 2024** |
| `person`, `address`, `cheese` | **person, address, cheese** — *giữ nguyên, không biến dạng* |

Dòng cuối mới là phần khó: `s` `f` `r` `x` `j` `w` và nguyên âm đôi trong tiếng Anh đều trùng với phím phụ của Telex. Telebit không thay ký tự một cách máy móc — nó dựng lại **âm tiết tiếng Việt** (*Âm đầu + Vần + Thanh*), kiểm tra kết quả có hợp lệ hay không, nếu không thì trả về đúng những phím bạn đã bấm.

→ Chi tiết: [Cấu trúc âm tiết](https://sonnam0904.github.io/telebit/concepts/vietnamese-syllable/) · [Kiểm tra chính tả & escape](https://sonnam0904.github.io/telebit/concepts/spell-check/)

<a id="-cài-đặt"></a>
<a id="installation"></a>

## ⬇️ Cài đặt

### Chọn cách phù hợp với bạn

| Hệ điều hành của bạn | Cách nên dùng |
|---|---|
| **Ubuntu 22.04 / 24.04 / 26.04** | ✅ [**APT repo**](#install-apt-repo) — cài 1 lệnh, tự cập nhật về sau |
| **Debian / Ubuntu bản khác** | [Gói `.deb` tải tay](#install-deb) |
| **Fedora / CentOS** | [Gói `.rpm`](#install-rpm) |
| **Arch / distro khác** | [Build từ source](#install-source) |

> [!TIP]
> Đang dùng **vnkey** cũ? Gỡ trước khi cài Telebit để hai bộ gõ không tranh nhau bàn phím → [Chuyển từ vnkey](https://sonnam0904.github.io/telebit/reference/migrate-from-vnkey/).

---

<a id="install-apt-repo"></a>

### Cách 1 — APT repo *(khuyên dùng)*

Thêm repo một lần, sau đó nhận bản mới qua `apt upgrade` như mọi gói khác:

```bash
# 1. Thêm khoá ký của repo
curl -fsSL https://sonnam0904.github.io/telebit/pubkey.gpg \
  | sudo gpg --dearmor -o /usr/share/keyrings/telebit-archive-keyring.gpg

# 2. Thêm repo (tự lấy codename: jammy 22.04 / noble 24.04 / resolute 26.04)
echo "deb [signed-by=/usr/share/keyrings/telebit-archive-keyring.gpg] https://sonnam0904.github.io/telebit $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/telebit.list

# 3. Cài
sudo apt update
sudo apt install telebit
```

> [!IMPORTANT]
> Repo hiện có 3 suite: **`jammy`** (Ubuntu 22.04), **`noble`** (24.04) và **`resolute`** (26.04). Chạy `lsb_release -cs` để kiểm tra máy bạn — nếu ra codename khác (ví dụ `bookworm` của Debian), `apt update` sẽ báo không tìm thấy, khi đó hãy dùng [Cách 2](#install-deb) và chọn file `.deb` gần nhất.

`telebit` là metapackage tiện dùng, chỉ phụ thuộc vào gói thật `telebit-fcitx5` — `apt install telebit-fcitx5` cũng cho kết quả y hệt.

**➡️ Cài xong? Chuyển sang [Bật fcitx5 và thêm input method](#-sau-khi-cài-3-bước-để-gõ-được).**

---

<a id="install-deb"></a>

<details>
<summary><b>Cách 2 — Gói <code>.deb</code> tải tay</b> (Debian / Ubuntu)</summary>

<br>

File `.deb` **không nằm trong repo**, mỗi bản được build tự động trên GitHub Actions.

**1. Tải file**

| Nguồn | Cách lấy |
|---|---|
| **Releases** *(nên dùng)* | [Trang Releases](https://github.com/sonnam0904/telebit/releases) → mỗi tag có **ba** `.deb`, hậu tố `+jammy` (22.04), `+noble` (24.04) hoặc `+resolute` (26.04) |
| **Artifacts** | **Actions** → **Release** → run mới nhất → **Artifacts** → `telebit-fcitx5-deb-jammy` / `-noble` / `-resolute` (giải nén ra `.deb`) |

Tên file dạng `telebit-fcitx5_<phiên-bản>+jammy_amd64.deb`. Chạy `lsb_release -cs` để biết nên lấy hậu tố nào — mỗi bản build trên chính Ubuntu đó nên khớp `libstdc++`/`libc` tương ứng.

**2. Cài**

```bash
cd ~/Downloads
sudo apt update
sudo apt install -y ./telebit-fcitx5_*_amd64.deb
```

> [!WARNING]
> Bắt buộc có `./` (hoặc đường dẫn tuyệt đối) để `apt` hiểu đây là file local chứ không phải tên gói trên mirror.

Nếu dùng `dpkg` và gặp lỗi thiếu dependency:

```bash
sudo dpkg -i ./telebit-fcitx5_*_amd64.deb
sudo apt-get install -f -y
```

**➡️ Tiếp tục: [Bật fcitx5 và thêm input method](#-sau-khi-cài-3-bước-để-gõ-được).**

</details>

<a id="install-rpm"></a>

<details>
<summary><b>Cách 3 — Gói <code>.rpm</code></b> (Fedora / CentOS)</summary>

<br>

Gói `.rpm` cũng được build tự động mỗi bản release, và yêu cầu hệ thống có sẵn `fcitx5` (nằm trong repo mặc định của Fedora).

**1. Tải file** — từ [Releases](https://github.com/sonnam0904/telebit/releases) hoặc **Actions → Artifacts**. Tên file dạng `telebit-fcitx5-*-fedora43.rpm`.

**2. Cài**

```bash
cd ~/Downloads
sudo dnf install -y ./telebit-fcitx5*.rpm
```

**➡️ Tiếp tục: [Bật fcitx5 và thêm input method](#-sau-khi-cài-3-bước-để-gõ-được).**

</details>

<a id="install-source"></a>
<a id="install-cmake-system"></a>
<a id="install-script"></a>

<details>
<summary><b>Cách 4 — Build từ source</b> (mọi distro: Arch, Debian, Fedora, …)</summary>

<br>

**1. Cài dependency**

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y \
  fcitx5 fcitx5-configtool fcitx5-config-qt fcitx5-module-lua \
  libfcitx5core-dev libfcitx5utils-dev libcurl4-openssl-dev \
  extra-cmake-modules cmake build-essential

# Fedora
sudo dnf install -y fcitx5 fcitx5-configtool fcitx5-devel libcurl-devel \
  gcc-c++ cmake make extra-cmake-modules

# Arch
sudo pacman -S --needed base-devel cmake extra-cmake-modules \
  fcitx5 fcitx5-configtool curl
```

Yêu cầu: compiler **C++17**, **CMake ≥ 3.10**, header phát triển của **fcitx5** và **libcurl** (libcurl dùng cho trợ lý AI).

**2a. Cách nhanh — script `install.sh`**

```bash
git clone https://github.com/sonnam0904/telebit.git
cd telebit

./install.sh            # = --system, cài vào /usr (mặc định, cần sudo)
./install.sh --user     # cài vào $HOME/.local
```

Script sẽ build core C++, chạy test, build addon rồi `cmake --install` theo prefix tương ứng.

**2b. Cách thủ công — CMake**

```bash
cd telebit-fcitx5

# System-wide (đề xuất)
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr .
cmake --build build
sudo cmake --install build
```

<a id="install-cmake-user"></a>

```bash
# Hoặc user-local, không cần sudo
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local" .
cmake --build build
cmake --install build
```

> [!CAUTION]
> **Hãy dùng `--system` / `PREFIX=/usr` trừ khi bạn biết rõ mình đang làm gì.** fcitx5 chỉ nạp addon `.so` từ thư mục lib addon compile-sẵn của chính nó (thường là `/usr/lib/<arch>/fcitx5`) — nó **không** tự dò `$HOME/.local/lib/fcitx5`. Nếu fcitx5 của bạn cài qua APT/`.deb`/`.rpm` (trường hợp phổ biến nhất) thì bản `--user` sẽ cài xong mà không được nhận diện. Chỉ dùng `--user` khi bạn tự build và chạy fcitx5 từ `$HOME/.local`.

**➡️ Tiếp tục: [Bật fcitx5 và thêm input method](#-sau-khi-cài-3-bước-để-gõ-được).**

</details>

---

<a id="-sau-khi-cài-3-bước-để-gõ-được"></a>

## 🚀 Sau khi cài: 3 bước để gõ được

Cài gói xong **chưa gõ được ngay** — fcitx5 cần được bật và Telebit cần được thêm vào danh sách input method.

### Bước 1 — Đặt fcitx5 làm input method của session

Bỏ qua bước này nếu bạn đã dùng fcitx5. Nếu máy đang dùng IBus, addon sẽ cài thành công nhưng **không hoạt động**.

```bash
# Ubuntu / Debian
im-config -n fcitx5
```

<details>
<summary>GNOME / KDE hoặc distro khác — cấu hình tay</summary>

Thêm vào `~/.profile`, `~/.xprofile`, hoặc file env của desktop:

```bash
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
```

</details>

Sau đó **logout / login** (hoặc reboot), rồi kiểm tra fcitx5 đang chạy:

```bash
fcitx5 -d
```

### Bước 2 — Thêm Telebit vào danh sách input method

```bash
fcitx5-configtool
```

1. Tab **Input Method** → bấm **Add**
2. Tìm `telebit-fcitx5` — tên hiển thị: **“Vietnamese Telex (UTF-8) - Telebit (telebit-fcitx5)”**
3. Thêm vào danh sách → **Apply**

> [!NOTE]
> Không thấy `telebit-fcitx5` trong danh sách? Chạy `fcitx5 -r` để restart. Nếu vẫn chưa có, **logout/login** hoặc reboot — lần cài đầu tiên thường cần việc này.

### Bước 3 — Thử gõ

Chuyển sang `telebit-fcitx5` bằng phím tắt của fcitx5, mở một ô nhập bất kỳ và gõ:

| Gõ | Phải ra |
|---|---|
| `tieengs vieetj` | tiếng việt |
| `nguyeenx` | nguyễn |

✅ Ra đúng là xong. ❌ Chưa đúng → [Xử lý sự cố](#-xử-lý-sự-cố).

---

## ⌨️ Chế độ gõ

Telebit có **2 chế độ hiển thị**, đổi trong `fcitx5-configtool` → tab **Addons** → **telebit-fcitx5** → **Configure**:

| | **Preedit** (gạch chân) | **Direct commit** *(mặc định)* |
|---|---|---|
| **Hiển thị** | Chữ đang gõ có gạch chân, chốt khi nhấn Space / Enter / dấu câu | Chữ Việt hiện **trực tiếp** trong ô nhập, không gạch chân |
| **Ưu điểm** | Rất ổn định, tương thích gần như mọi ứng dụng | Tự nhiên hơn — thấy `nguyeenx` biến dần thành `nguyễn` |
| **Đánh đổi** | Có gạch chân trong lúc gõ | Undo/Redo ở vài ứng dụng hoạt động hơi khác |
| **Bật bằng** | Bỏ tích `DirectCommitRollback` | Tích `DirectCommitRollback` |

Với ứng dụng không hỗ trợ đủ tính năng fcitx5, Telebit **tự quay về chế độ preedit** để tránh lỗi.

→ Chi tiết: [Chế độ gõ](https://sonnam0904.github.io/telebit/guide/typing-modes/)

<a id="-tuỳ-chọn-cấu-hình"></a>

## ⚙️ Tuỳ chọn cấu hình

`fcitx5-configtool` → **Addons** → **telebit-fcitx5** → **Configure**:

| Tuỳ chọn | Mặc định | Ý nghĩa |
|---|:---:|---|
| **DirectCommitRollback** | Bật | Chèn chữ trực tiếp thay vì preedit gạch chân (xem bảng trên). |
| **SpellCheckRestore** | Bật | Từ vừa gõ không phải âm tiết tiếng Việt hợp lệ thì khôi phục phím gốc — giúp gõ xen tiếng Anh (`person`, `address`, `cheese`) không bị biến dạng. |
| **VNIMode** | Tắt | Chuyển sang kiểu **VNI**: `1-5` = sắc/huyền/hỏi/ngã/nặng, `0` = xoá thanh, `6` = mũ (â/ê/ô), `7` = móc (ư/ơ), `8` = ă, `9` = đ. Ví dụ `vie65t` → `việt`, `d9i` → `đi`. Gõ đúp số để ra số literal (`a11` → `a1`); đuôi số như `nam2024` giữ nguyên. |
| **ModernToneStyle** | Tắt | Đặt dấu kiểu mới cho vần `oa/oe/uy`: `hoá, khoẻ, thuý` thay vì kiểu cũ `hóa, khỏe, thúy`. |
| **AutoCapitalizeSentence** | Bật | Tự viết hoa chữ đầu câu sau `.` `?` `!` rồi Space/Enter. |
| **ToggleVietnameseKey** | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | Tạm bật/tắt gõ tiếng Việt — tiện khi cần gõ một đoạn tiếng Anh dài. |
| **Macros** | *(trống)* | Gõ tắt: khai báo cặp *viết tắt → nội dung* (vd `vn` → `Việt Nam`). Thay thế khi kết thúc từ. |
| **AIEnabled** | **Tắt** | Bật trợ lý AI (xem mục dưới). |

→ Chi tiết: [Tuỳ chọn cấu hình](https://sonnam0904.github.io/telebit/guide/configuration/) · [Cách gõ Telex & VNI](https://sonnam0904.github.io/telebit/guide/telex-vni/) · [Gõ tắt](https://sonnam0904.github.io/telebit/guide/macros/)

## 🤖 Trợ lý AI

Nhấn <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Space</kbd> ngay trong lúc gõ để mở ô yêu cầu, viết prompt — dán thêm ngữ cảnh bằng <kbd>Ctrl</kbd>+<kbd>V</kbd> nếu muốn — rồi <kbd>Enter</kbd>. Văn bản AI sinh ra được chèn thẳng vào ứng dụng. Dùng tốt cho: soạn/trả lời tin nhắn, dịch, tóm tắt, sửa câu.

**Cần 2 việc để dùng được:**

1. Bật **AIEnabled** trong `fcitx5-configtool` (mặc định **tắt**).
2. Khai báo **biến môi trường** — API key, endpoint, model… Không có tham số nào lưu trong file config, để tránh rò rỉ key:

   ```bash
   AI_API_KEY  AI_ENDPOINT  AI_MODEL  AI_SYSTEM_PROMPT  AI_MAX_TOKENS
   ```

Bạn cũng có thể tạo sẵn **skill** (`/tên-skill`) để tái dùng các hướng dẫn hay lặp lại.

→ 📖 **Hướng dẫn đầy đủ: [Trợ lý AI](https://sonnam0904.github.io/telebit/ai-assistant/)**

---

<a id="-xử-lý-sự-cố"></a>

## 🩺 Xử lý sự cố

| Hiện tượng | Thử trước |
|---|---|
| Không thấy `telebit-fcitx5` khi bấm **Add** | `fcitx5 -r`, chưa được thì logout/login hoặc reboot |
| Cài xong nhưng gõ không ra chữ Việt | Kiểm tra fcitx5 đang là IM của session: `im-config -n fcitx5` → logout/login |
| Chữ bị nhân đôi hoặc nhảy lộn xộn | Bỏ tích **DirectCommitRollback** để về chế độ preedit |
| Tiếng Anh bị biến dạng | Bật **SpellCheckRestore**, hoặc <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> để tắt tạm |
| Cài `--user` nhưng fcitx5 không nhận | Cài lại bằng `--system` (xem cảnh báo ở [Cách 4](#install-source)) |
| Bộ gõ vnkey cũ vẫn còn tranh bàn phím | [Chuyển từ vnkey](https://sonnam0904.github.io/telebit/reference/migrate-from-vnkey/) |

→ Đầy đủ hơn: [Xử lý sự cố](https://sonnam0904.github.io/telebit/reference/troubleshooting/) · Vẫn tắc? [Mở issue](https://github.com/sonnam0904/telebit/issues/new)

---

## 🗑️ Gỡ cài đặt

<details>
<summary><b>Bấm để xem lệnh gỡ theo từng cách đã cài</b></summary>

<br>

**Cài bằng APT repo hoặc gói `.deb`:**

```bash
sudo apt remove telebit telebit-fcitx5
fcitx5 -r
```

Gỡ luôn cả APT repo:

```bash
sudo rm -f /etc/apt/sources.list.d/telebit.list \
           /usr/share/keyrings/telebit-archive-keyring.gpg
sudo apt update
```

**Cài bằng gói `.rpm`:**

```bash
sudo dnf remove telebit-fcitx5
fcitx5 -r
```

**Cài từ source vào `/usr` (system-wide):**

```bash
# Ubuntu/Debian đặt addon trong thư mục theo kiến trúc, distro khác thì không —
# glob này khớp cả hai
sudo rm -f /usr/lib/fcitx5/telebit-fcitx5.so /usr/lib/*/fcitx5/telebit-fcitx5.so
sudo rm -f /usr/share/fcitx5/addon/telebit-fcitx5.conf
sudo rm -f /usr/share/fcitx5/inputmethod/telebit-fcitx5.conf
sudo rm -f /usr/share/icons/hicolor/scalable/apps/telebit-fcitx5.svg
fcitx5 -r
```

**Cài từ source vào `$HOME/.local` (user-local):**

```bash
rm -f "$HOME/.local/lib/fcitx5/telebit-fcitx5.so"
rm -f "$HOME/.local/share/fcitx5/addon/telebit-fcitx5.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/telebit-fcitx5.conf"
rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/telebit-fcitx5.svg"
fcitx5 -r
```

→ Chi tiết: [Gỡ cài đặt](https://sonnam0904.github.io/telebit/reference/uninstall/)

</details>

## 🛠️ Dành cho người phát triển

<details>
<summary><b>Build từ source, chạy test, cấu trúc code</b></summary>

<br>

### Build core C++ và chạy test

Core không cần fcitx5 hay libcurl — chỉ compiler C++17 và CMake ≥ 3.10:

```bash
cmake -B build .
cmake --build build
./build/telebit_telex_tests
```

Chạy đúng sẽ in:

```text
All C++ tests passed.
```

Build sạch khi đổi nhánh hoặc pull thay đổi lớn:

```bash
rm -rf build && cmake -B build . && cmake --build build && ./build/telebit_telex_tests
```

### Cấu trúc source

| Đường dẫn | Vai trò |
|---|---|
| `vietnamese.h/.cpp` | API công khai `telex_to_unicode(const std::string&)` — Telex → Unicode |
| `engine.h/.cpp` | Lớp `EngineVietCpp` — quản lý buffer gõ theo từng phím, dùng trong fcitx5 |
| `canonicalize.*` | Pipeline canonicalize input: escape rules, tách âm đầu/vần, chuẩn hoá `w`/`aa`/`ee`/`oo`, trích xuất thanh |
| `rime_table.*` | Bảng vần + vị trí “nguyên âm chính” để đặt dấu |
| `render_utf8.*` | Render biểu diễn nội bộ → Unicode UTF‑8 (dấu, chữ hoa…) |
| `ai_client.*` | HTTP client cho trợ lý AI (libcurl) |
| `telebit-fcitx5/` | Mã nguồn addon fcitx5 |
| `tests.cpp` | Bộ test của core |
| `scripts/` | Script đóng gói: `build-deb.sh`, `build-rpm.sh`, `apt-repo-publish.sh`… |
| `install.sh` | Cài nhanh: build core + addon rồi install |

### Dùng engine trong project khác

Core hoàn toàn độc lập với fcitx5:

```cpp
#include "vietnamese.h"

std::string out = telex_to_unicode("tieengs vieetj");  // "tiếng việt"
```

→ Chi tiết: [Build & test](https://sonnam0904.github.io/telebit/dev/build-and-test/) · [Kiến trúc](https://sonnam0904.github.io/telebit/dev/architecture/)

</details>

---

## 🤝 Đóng góp

Rất hoan nghênh issue và pull request. Xem [CONTRIBUTING.md](CONTRIBUTING.md) và [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Báo lỗi bảo mật theo [SECURITY.md](SECURITY.md).

## 📄 Giấy phép

Phát hành dưới [MIT License](LICENSE).

---

## ✨ Star History

<a href="https://star-history.com/#sonnam0904/telebit&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=sonnam0904/telebit&type=date&legend=top-left" />
 </picture>
</a>

<div align="center"><sub><a href="#readme-top">↑ Về đầu trang</a></sub></div>
