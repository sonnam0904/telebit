# Gỡ bộ gõ **vnkey** cũ (trước khi cài Telebit)

Nếu máy từng cài addon **`vnkey`** (file kiểu `vnkey.so`, `addon/vnkey.conf`, `inputmethod/vnkey.conf`, gói **`vnkey-fcitx5`**, v.v.), nên **dọn hết** rồi mới cài **`telebit-fcitx5`** — tránh đè file, trùng slot trong fcitx5 và mang theo cấu hình cũ.

## Gỡ bằng gói

**Debian / Ubuntu** (tên gói phổ biến của bản branding cũ):

```bash
sudo apt remove vnkey-fcitx5
sudo apt autoremove
```

Nếu bạn cài từ nguồn khác (PPA, `.deb` tay), dùng đúng tên gói trong `apt list --installed | grep -i vnkey`.

**Fedora / RPM:**

```bash
sudo dnf remove vnkey-fcitx5
```

(bỏ qua nếu gói không tồn tại.)

## Gỡ file còn sót (cài tay hoặc build cũ)

**Toàn hệ thống** — đường dẫn `vnkey.so` tuỳ distro (thử lệnh `find` bên dưới nếu không chắc):

```bash
sudo rm -f /usr/lib/x86_64-linux-gnu/fcitx5/vnkey.so /usr/lib/fcitx5/vnkey.so /usr/lib64/fcitx5/vnkey.so
sudo rm -f /usr/share/fcitx5/addon/vnkey.conf
sudo rm -f /usr/share/fcitx5/inputmethod/vnkey.conf
```

```bash
# Nếu vẫn thấy vnkey đâu đó:
sudo find /usr -name 'vnkey.so' 2>/dev/null
```

**Theo user** (`$HOME/.local`):

```bash
rm -f "$HOME/.local/lib/fcitx5/vnkey.so"
rm -f "$HOME/.local/share/fcitx5/addon/vnkey.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/vnkey.conf"
```

## Trong fcitx5-configtool

1. Mở **fcitx5-configtool** → tab **Input Method**.
2. **Xoá** khỏi danh sách mọi mục liên quan **vnkey** / IME cũ (đừng để trùng với Telebit).
3. Tuỳ chọn: xóa file cấu hình cũ `~/.config/fcitx5/conf/vnkey.conf` nếu không còn dùng.

## Khởi động lại fcitx5

```bash
fcitx5 -r
```

Sau đó quay lại [Cài đặt](../getting-started/installation.md) để cài **Telebit** (`telebit-fcitx5`).
