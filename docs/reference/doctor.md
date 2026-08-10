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
│ Ứng dụng cài trực tiếp (.deb/.rpm)                                   │
├───┬──────────────────────────┬───────────────────────────────────────┤
│ ✔ │ Module GTK trên host     │ GTK3 fcitx5 · GTK4 fcitx5             │
│ ✔ │ Module Qt trên host      │ Qt5 fcitx5 · Qt6 fcitx5               │
│ ✔ │ Cache immodule GTK3      │ có fcitx                              │
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


## Bốn khối trong báo cáo



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

### Ứng dụng cài trực tiếp (.deb/.rpm)

Khối này nói về mọi ứng dụng cài thẳng lên máy — `apt`, `dnf`, `pacman`, không phải sandbox.
Ứng dụng loại này gõ được hay không phụ thuộc vào **module client** của fcitx5 cho đúng toolkit
mà nó dùng, và mỗi toolkit là một gói cài riêng:

| Ô trong bảng | Gói cần cài (Debian/Ubuntu) | Fedora / openSUSE / Arch |
| ------------ | --------------------------- | ------------------------ |
| `GTK3 thiếu` | `fcitx5-frontend-gtk3`      | `fcitx5-gtk`             |
| `GTK4 thiếu` | `fcitx5-frontend-gtk4`      | `fcitx5-gtk`             |
| `Qt5 thiếu`  | `fcitx5-frontend-qt5`       | `fcitx5-qt`              |
| `Qt6 thiếu`  | `fcitx5-frontend-qt6`       | `fcitx5-qt`              |

Qt5 và Qt6 được tách riêng vì chúng là hai gói độc lập: máy chỉ có Qt6 mà thiếu plugin Qt5
thì không có gì phải sửa, nên ô đó là `n/a` chứ không phải `thiếu`.

Khác với sandbox, thiếu module ở đây **sửa được** — và mức độ phụ thuộc vào biến môi trường
của session:

| Tình huống                                            | Kết quả                                                                                                          |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `GTK_IM_MODULE=fcitx` (hoặc `QT_IM_MODULE`) mà module tương ứng không có, **và có app dùng toolkit đó** | `✘` **Đang hỏng.** Biến trỏ tới module không tồn tại nên toolkit rơi về bộ gõ mặc định của nó — **kể cả trên Wayland**, vì biến này chặn luôn đường text-input-v3. |
| Cùng tình huống, nhưng **không app nào** dùng toolkit đó và scan kết luận được hết | `!` Rủi ro chưa xảy ra, exit code vẫn 0. Cài một app dùng toolkit đó thì nó thành lỗi thật. |
| Biến để trống, session Wayland                        | `·` Không sao, ứng dụng đi thẳng text-input-v3 qua compositor.                                                   |
| Biến để trống, session X11                            | `!` Rủi ro; phần thiếu biến đã được khối **Phiên làm việc** báo riêng.                                           |

Khi có ô `thiếu`, doctor liệt kê luôn **app nào trên máy đang dùng toolkit đó**, để dòng đó
không dừng ở mức "rủi ro":

```
│ ✘ │ Module GTK trên host │ GTK3 fcitx5 · GTK4 thiếu                              │
│   │                      │ ↳ GTK_IM_MODULE=fcitx trỏ tới module GTK4 không có…   │
│ ✘ │   app dùng GTK4      │ Calculator, Files, Fonts, Online Accounts … và 1 nữa  │
│   │                      │ ↳ Dò 71 ứng dụng cài trực tiếp; 13 app không đọc được │
│   │                      │   toolkit nên danh sách này có thể còn thiếu.         │
```

Danh sách được dựng từ desktop entry trong `/usr/share/applications`, `/usr/local/share/applications`
và `~/.local/share/applications` (không tính Flatpak/Snap — chúng thuộc khối sau), rồi đọc thư viện
thật của từng binary bằng **một** lệnh `ldd` cho toàn bộ danh sách. Chỉ chạy khi có ô `thiếu`, nên
máy đủ module không phải trả phí này.

Hai điều danh sách **không** làm, và lý do:

- Nó chỉ khẳng định chiều dương ("app này có dùng GTK4"), không bao giờ khẳng định chiều âm. App
  bọc bằng script (`soffice`) hoặc tự `dlopen` toolkit (Firefox nạp GTK qua `libxul`) không đọc
  được toolkit từ binary, nên chúng được **đếm** vào phần "không đọc được" chứ không bị kết luận
  là không ảnh hưởng. Cùng nhóm đó là app khởi động qua interpreter (`env`, `sh`, `python3`,
  `gjs`) và app nằm ngoài `/usr`, `/opt`, `/bin`, `/sbin`: `ldd` phải chạy ELF interpreter của
  file nó xem, nên doctor không đưa binary ngoài các prefix hệ thống cho nó — desktop entry đến
  từ `~/.local/share/applications` là chỗ người dùng ghi được.
- Nó cắt danh sách ở 8 tên, nhưng luôn nói rõ còn bao nhiêu app nữa — danh sách bị cắt lặng lẽ
  sẽ bị đọc thành danh sách đầy đủ.

Chính vì chỉ khẳng định chiều dương, kết quả scan có quyền **hạ** mức nghiêm trọng chứ không
được xoá nó:

- Không app nào dùng toolkit đang thiếu **và** mọi app đều đọc được toolkit → dòng cha hạ từ `✘`
  xuống `!`, dòng con là `·`, exit code về 0. Máy không hỏng, chỉ đang có một cấu hình chờ hỏng.
- Không app nào dùng toolkit đang thiếu **nhưng** còn app không đọc được toolkit → giữ `✘`. "Không
  app nào dùng GTK4" lúc đó chỉ là phỏng đoán, mà phỏng đoán thì không được tha một lỗi.
- Scan không dò được app nào (không có `ldd`, không đọc được `/usr/share/applications`, chạy trong
  container) → giữ `✘`, và dòng con nói thẳng "không dò được app nào". Danh sách rỗng không phải
  bằng chứng.
- Một họ toolkit có hai ô `thiếu` (ví dụ GTK3 có app dùng, GTK4 không) → vẫn `✘`, vì một trong
  hai ô đã có nạn nhân.

Dòng **Cache immodule GTK3** là một lỗi âm thầm mà không chỗ nào khác trong báo cáo thấy được:
GTK3 chỉ nạp immodule nào có trong `immodules.cache`, nên file `im-fcitx5.so` nằm trên đĩa vẫn
có thể không bao giờ được nạp nếu cache được ghi trước khi module xuất hiện (copy tay, hoặc
trigger của gói không chạy). GTK4 không cần kiểm tra tương tự — nó quét thẳng thư mục immodules.

Phép so là **với đúng tên file đang cài**, không phải với chữ "fcitx": cache còn giữ entry
`im-fcitx.so` của gói fcitx4 trong khi `im-fcitx5.so` mới là cái đang cài vẫn là cache cũ, và
doctor gọi thẳng tên file mà cache đang trỏ tới để bạn biết cần dựng lại nó:

```
│ ✘ │ Cache immodule GTK3  │ đăng ký im-fcitx.so                                   │
│   │                      │ ↳ Module im-fcitx5.so có trên đĩa nhưng cache không    │
│   │                      │   đăng ký nó… Cache đang đăng ký im-fcitx.so — bản     │
│   │                      │   module của gói cũ.                                  │
```

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
| `✘ Module GTK/Qt trên host — … thiếu`    | Session đang yêu cầu một module client chưa được cài, **và** có app native dùng toolkit đó — dòng con liệt kê chính các app đang hỏng                                                        | Cài gói module theo [bảng ở trên](#ung-dung-cai-truc-tiep-debrpm) — doctor in ra đúng lệnh cần chạy                |
| `! Module GTK/Qt trên host — … thiếu`    | Cùng chuyện, nhưng chưa app nào dùng toolkit đó. Chưa hỏng, sẽ hỏng khi bạn cài một app như vậy                                                                                              | Cài trước cho yên tâm, hoặc bỏ qua đến khi thật cần                                                                |
| `✘ Cache immodule GTK3 — đăng ký im-fcitx.so` | Cache còn giữ entry của gói fcitx4 cũ, `im-fcitx5.so` đang cài thì không được đăng ký nên GTK3 không nạp nó                                                                            | `sudo apt install --reinstall fcitx5-frontend-gtk3`, hoặc chạy lại `gtk-query-immodules-3.0 --update-cache`         |




## `--deep` — khi nào cần

```bash
telebit doctor --deep
```

Mặc định doctor không vào trong sandbox nào: nó đọc filesystem, hỏi D-Bus, và — chỉ khi
có ô `thiếu` — chạy một lệnh `ldd` để biết app native nào dùng toolkit đó. `--deep` thì
khởi động một shell dùng một lần bên trong **từng** ứng dụng sandbox để xem biến môi
trường thật sự tới nơi hay không.

Dùng khi một ứng dụng cụ thể không gõ được trong khi các ứng dụng khác cùng runtime thì
bình thường — nguyên nhân hay gặp là ứng dụng đó có `flatpak override` riêng.

## `--markdown` — khi báo lỗi

```bash
telebit doctor --markdown
```

In bản không màu, dạng bảng, để dán vào [issue trên GitHub](https://github.com/sonnam0904/telebit/issues/new).

!!! tip "Cần chi tiết phần host?"

    `telebit doctor` cố tình không đụng tới locale và cấu hình chi tiết của fcitx5 —
    `fcitx5-diagnose` (cài kèm fcitx5) đã làm những thứ đó rất kỹ. Hai lệnh bổ sung cho nhau:
    doctor lo phiên đồ hoạ, module client trên host, ứng dụng native và phần sandbox;
    `fcitx5-diagnose` lo phần còn lại.

