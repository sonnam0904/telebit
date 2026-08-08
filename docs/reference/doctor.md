# `telebit doctor`

Khi gõ được ở app này nhưng không gõ được ở app kia, `telebit doctor` chỉ ra chỗ hỏng. Nó đặc biệt hữu ích với ứng dụng Snap / Flatpak — nơi mà lỗi thường **không** nằm ở Telebit hay fcitx5, và có những trường hợp không sửa được.

## Cách sử dụng

```bash
telebit doctor
```

Kết quả trông như thế này:

```
┌──────────────────────────────────────────────────────────────────────┐
│ fcitx5                                                               │
├───┬──────────────────────────┬───────────────────────────────────────┤
│ ✔ │ Tiến trình               │ đang chạy 5.1.7                       │
│ ✔ │ Addon Telebit            │ /usr/lib/…/fcitx5/telebit-fcitx5.so   │
│ · │ Frontend đang mở         │ native, portal, fcitx4, ibus          │
└───┴──────────────────────────┴───────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│ Ứng dụng đóng hộp (sandbox)                                          │
├───┬──────────────────────────┬───────────────────────────────────────┤
│ ✔ │ flatpak:org.gnome.Plat…  │ GTK3 fcitx5 · GTK4 fcitx5 · Qt n/a    │
│ · │ org.gnome.meld           │ override GTK_IM_MODULE=fcitx          │
│ ! │ snap:gnome-46-2404       │ GTK3 fcitx4 · GTK4 thiếu · Qt thiếu   │
│   │                          │ ↳ Module GTK3 là bản fcitx4…          │
│ · │ chromium                 │ —                                     │
│ · │ firefox                  │ —                                     │
└───┴──────────────────────────┴───────────────────────────────────────┘
```

Ký hiệu ở cột đầu:


|     | Nghĩa                                                        |
| --- | ------------------------------------------------------------ |
| `✔` | bình thường                                                  |
| `!` | chạy được nhưng không tối ưu, **hoặc** là rủi ro chưa xảy ra |
| `✘` | đang hỏng ngay lúc này                                       |
| `·` | Chỉ là thông tin, không phải kết quả phân tích               |


## Ba khối trong báo cáo



### Phiên làm việc

Biến IM ở đây **không phải** của terminal bạn đang gõ, mà đọc từ tiến trình compositor —
tức là môi trường các ứng dụng thật sự nhận. Hai cái khác nhau là chuyện thường, thường
do terminal được mở từ trước lần đăng nhập gần nhất; doctor sẽ nói ra khi gặp.

`GTK_IM_MODULE` để trống **không phải lúc nào cũng sai**:


| Session                  | Để trống thì                                                           |
| ------------------------ | ---------------------------------------------------------------------- |
| X11                      | **Hỏng.** GTK không nạp module nào của fcitx5.                         |
| Wayland + KWin, COSMIC, sway, niri, Hyprland… | **Đúng.** Các compositor này hiện thực `zwp_input_method_v2` nên fcitx5 gắn thẳng vào; fcitx5 khuyến nghị để trống. |
| Wayland + mutter (GNOME) | Chạy được, miễn là frontend `ibus` đang mở (xem dưới).                 |




### fcitx5

Dòng **Frontend đang mở** là các cửa vào fcitx5. Thiếu cửa nào thì nhóm ứng dụng tương
ứng không gõ được:


| Frontend | Ai đi qua cửa này                |
| -------- | -------------------------------- |
| `native` | ứng dụng cài thẳng trên máy      |
| `portal` | **mọi ứng dụng Flatpak / Snap**  |
| `fcitx4` | ứng dụng GTK3 đóng gói Snap      |
| `ibus`   | ứng dụng chạy trên GNOME Wayland |


Bật/tắt các frontend trong `fcitx5-configtool` → **Addons**.

### Ứng dụng sandbox

Mỗi dòng ở mức ngoài cùng là một **runtime** (Flatpak) hoặc **platform snap**; các dòng thụt
vào dưới nó là ứng dụng đang chạy trên đó. Phán quyết gắn với runtime chứ không với từng ứng
dụng, vì từ bên ngoài không thể biết một ứng dụng dùng GTK3, GTK4 hay Qt.

Ngoại lệ duy nhất là nhóm **Không gắn được runtime** ở cuối: ứng dụng tự đóng gói toolkit
riêng (Firefox snap chẳng hạn) không nằm trên runtime nào doctor nhận ra. Chúng vẫn được liệt
kê để báo cáo không bỏ sót thứ đã cài, nhưng phán quyết về module ở trên không áp dụng cho
chúng.

Trong bảng module, hai chữ dễ nhầm:

- `thiếu` — runtime *có* toolkit đó nhưng *không có* module fcitx cho nó. **Rủi ro, chưa chắc là
  lỗi đang xảy ra**: từ bên ngoài doctor không biết ứng dụng nào bên dưới thật sự dùng toolkit
  đó, nên dòng này chỉ lên `!`, không bao giờ lên `✘`. Chỉ xử lý khi có một app cụ thể không gõ
  được (xem [bảng dưới](#gap-loi-thi-lam-gi)).
- `n/a` — runtime không hề có toolkit đó. Không có gì để sửa.



## Gặp lỗi thì làm gì


| Dòng bạn thấy                            | Nghĩa là                                                                                                                                                                                     | Xử lý                                                                                                              |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `✘ IM env của session` — trỏ sang `ibus` | File khác đang ghi đè cấu hình của Telebit                                                                                                                                                   | Xem `/etc/environment` và các file `environment.d` sắp sau `60-`                                                   |
| `✘ Tiến trình — không chạy`              | fcitx5 chưa khởi động                                                                                                                                                                        | `fcitx5 -d`                                                                                                        |
| `✘ Addon Telebit — không tìm thấy`       | Addon chưa cài, hoặc cài vào `~/.local` trong khi fcitx5 đến từ APT                                                                                                                          | [Cài lại toàn hệ thống](../getting-started/installation.md)                                                        |
| `✘ Frontend fcitx4 — tắt`                | Ứng dụng GTK3 đóng gói Snap sẽ không gõ được                                                                                                                                                 | Bật **Fcitx4 Frontend** trong `fcitx5-configtool` → Addons, rồi `fcitx5 -r`                                        |
| `! Frontend portal — tắt`                | Không ứng dụng sandbox nào tới được fcitx5                                                                                                                                                   | Bật **DBus Frontend** trong Addons                                                                                 |
| `! snap … GTK4 thiếu`                    | **Rủi ro, chưa chắc là lỗi.** Runtime đó không có module cho GTK4/Qt, nhưng doctor không biết ứng dụng nào bên dưới dùng GTK4/Qt — app GTK3 (Chromium, Firefox, Electron) vẫn gõ bình thường | Chỉ cần xử lý khi một app cụ thể thật sự không gõ được: dùng bản `.deb` của app đó, hoặc chuyển sang phiên Wayland |
| `! … interface chưa nối`                 | Snap thiếu quyền                                                                                                                                                                             | Chạy đúng lệnh `snap connect` mà doctor gợi ý                                                                      |




## `--deep` — khi nào cần

```bash
telebit doctor --deep
```

Mặc định doctor chỉ đọc filesystem, đủ để trả lời "module có tồn tại không". `--deep`
khởi động một shell dùng một lần bên trong **từng** ứng dụng để xem biến môi trường
thật sự tới nơi hay không.

Dùng khi một ứng dụng cụ thể không gõ được trong khi các ứng dụng khác cùng runtime thì
bình thường — nguyên nhân hay gặp là ứng dụng đó có `flatpak override` riêng.

## `--markdown` — khi báo lỗi

```bash
telebit doctor --markdown
```

In bản không màu, dạng bảng, để dán vào [issue trên GitHub](https://github.com/sonnam0904/telebit/issues/new).

!!! tip "Cần chi tiết phần host?"

    `telebit doctor` cố tình không đụng tới locale, `ldd`, cache immodule — `fcitx5-diagnose`
    (cài kèm fcitx5) đã làm những thứ đó rất kỹ. Hai lệnh bổ sung cho nhau: doctor lo phần
    sandbox và phiên đồ hoạ, `fcitx5-diagnose` lo phần host.

