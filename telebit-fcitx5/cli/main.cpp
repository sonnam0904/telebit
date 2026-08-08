// The `telebit` command. Today it carries a single subcommand, `doctor`; the
// subcommand shape is there so later additions (dictionary sync, config) do not
// have to claim new names in /usr/bin.

#include <iostream>
#include <string>

#include "doctor.h"

#ifndef TELEBIT_VERSION
#define TELEBIT_VERSION "unknown"
#endif

namespace {

void print_usage() {
    std::cout << "telebit " TELEBIT_VERSION " — bộ gõ tiếng Việt cho fcitx5\n"
                 "\n"
                 "Cách dùng:\n"
                 "  telebit doctor [--deep] [--markdown]\n"
                 "  telebit --version\n"
                 "\n"
                 "doctor — kiểm tra đường đi của input method, tập trung vào những chỗ\n"
                 "fcitx5-diagnose không nhìn tới: bên trong sandbox Flatpak/Snap, môi\n"
                 "trường thật của phiên đồ hoạ, và compositor đang chạy.\n"
                 "\n"
                 "  --deep      Vào thật bên trong từng sandbox để đọc biến môi trường.\n"
                 "              Chính xác nhất nhưng tốn vài giây cho mỗi ứng dụng.\n"
                 "  --markdown  In bản không màu, dạng bảng, để dán vào báo lỗi.\n";
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }

    const std::string command = argv[1];
    if (command == "--help" || command == "-h" || command == "help") {
        print_usage();
        return 0;
    }
    if (command == "--version" || command == "-V") {
        std::cout << TELEBIT_VERSION << "\n";
        return 0;
    }

    if (command == "doctor") {
        telebit::doctor::Options options;
        for (int i = 2; i < argc; ++i) {
            const std::string flag = argv[i];
            if (flag == "--deep") options.deep = true;
            else if (flag == "--markdown") options.markdown = true;
            else {
                std::cerr << "telebit doctor: tuỳ chọn không hiểu: " << flag << "\n";
                return 2;
            }
        }
        return telebit::doctor::run(options);
    }

    std::cerr << "telebit: lệnh không hiểu: " << command << "\n\n";
    print_usage();
    return 2;
}
