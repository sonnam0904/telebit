# Gỡ cài đặt

Chọn đúng cách bạn đã cài.

## Cài bằng APT repo hoặc gói `.deb`

```bash
sudo apt remove telebit telebit-fcitx5
fcitx5 -r
```

Muốn gỡ luôn cả APT repo:

```bash
sudo rm -f /etc/apt/sources.list.d/telebit.list \
           /usr/share/keyrings/telebit-archive-keyring.gpg
sudo apt update
```

## Cài bằng gói `.rpm`

```bash
sudo dnf remove telebit-fcitx5
fcitx5 -r
```

## Cài vào `/usr` (system-wide, bằng CMake hoặc `install.sh`)

```bash
sudo rm -f /usr/lib/fcitx5/telebit-fcitx5.so
sudo rm -f /usr/share/fcitx5/addon/telebit-fcitx5.conf
sudo rm -f /usr/share/fcitx5/inputmethod/telebit-fcitx5.conf
fcitx5 -r
```

!!! note "Đường dẫn `.so` có thể khác"

    Một số distro đặt addon trong thư mục theo kiến trúc. Nếu lệnh trên không tìm thấy file:

    ```bash
    ls /usr/lib/*/fcitx5/telebit-fcitx5.so
    ```

## Cài vào `$HOME/.local` (user-local)

```bash
rm -f "$HOME/.local/lib/fcitx5/telebit-fcitx5.so"
rm -f "$HOME/.local/share/fcitx5/addon/telebit-fcitx5.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/telebit-fcitx5.conf"
fcitx5 -r
```

---

## Xoá cấu hình còn sót (tuỳ chọn)

Cấu hình của addon (macro, danh sách ứng dụng, các tuỳ chọn) nằm trong config của fcitx5 và
**không** bị xoá theo gói:

```bash
rm -f "$HOME/.config/fcitx5/conf/telebit-fcitx5.conf"
```

Giữ lại file này nếu bạn có ý định cài lại — macro và tuỳ chọn sẽ còn nguyên.

## Bỏ Telebit khỏi danh sách input method

Sau khi gỡ, mở `fcitx5-configtool` → tab **Input Method** → chọn Telebit → **Remove**, rồi
**Apply**. Nếu không làm, danh sách có thể còn một mục trỏ tới addon không còn tồn tại.
