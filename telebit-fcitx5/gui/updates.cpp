#include "updates.h"

#include <curl/curl.h>
#include <glib.h>

#include <cstdlib>
#include <mutex>
#include <sstream>
#include <vector>

#ifndef TELEBIT_VERSION
#define TELEBIT_VERSION "unknown"
#endif

namespace telebit::setup::update {

namespace {

// The Debian package name. The metapackage `telebit` only depends on this one,
// so upgrading this is what actually replaces the addon; upgrading the
// metapackage would be a no-op on a machine that installed the real package
// directly.
constexpr const char *kPackage = "telebit-fcitx5";

constexpr const char *kReleasesApi =
    "https://api.github.com/repos/sonnam0904/telebit/releases/latest";
constexpr const char *kReleasesPage = "https://github.com/sonnam0904/telebit/releases";

// Short enough that a window opened on a captive-portal network is not stuck
// waiting for it, long enough for a slow mobile connection to answer.
constexpr long kTimeoutMs = 6000;

std::size_t write_callback(char *data, std::size_t size, std::size_t count, void *user) {
    auto *out = static_cast<std::string *>(user);
    const std::size_t total = size * count;
    // A release payload is a few kilobytes; anything far larger is not the
    // endpoint we asked for, and there is no reason to buffer it.
    if (out->size() + total > 512 * 1024) return 0;
    out->append(data, total);
    return total;
}

std::string trim(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::vector<int> version_parts(const std::string &version) {
    std::vector<int> parts;
    std::string digits;
    for (char ch : version) {
        if (ch >= '0' && ch <= '9') {
            digits += ch;
            continue;
        }
        if (ch == '.') {
            parts.push_back(digits.empty() ? 0 : std::atoi(digits.c_str()));
            digits.clear();
            continue;
        }
        // Anything else ends the version core: "+noble", "~rc1", "-1" are
        // packaging decoration, not something to compare numerically.
        break;
    }
    if (!digits.empty()) parts.push_back(std::atoi(digits.c_str()));
    return parts;
}

// Runs a command and returns its stdout, or an empty string if it could not be
// run at all. Used only to interrogate package databases, never to change them.
std::string run_capture(const std::vector<std::string> &argv) {
    std::vector<char *> args;
    args.reserve(argv.size() + 1);
    for (const auto &arg : argv) args.push_back(const_cast<char *>(arg.c_str()));
    args.push_back(nullptr);

    char *stdout_text = nullptr;
    int exit_status = 0;
    GError *error = nullptr;
    const gboolean ok = g_spawn_sync(nullptr, args.data(), nullptr,
                                     static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH |
                                                              G_SPAWN_STDERR_TO_DEV_NULL),
                                     nullptr, nullptr, &stdout_text, nullptr, &exit_status,
                                     &error);
    std::string out;
    // g_spawn_check_wait_status, not the _exit_status spelling: the latter is
    // deprecated at GLib 2.70 and gui/CMakeLists.txt turns deprecations into
    // errors, which is what keeps this building on jammy through trixie.
    if (ok != 0 && g_spawn_check_wait_status(exit_status, nullptr) != 0 &&
        stdout_text != nullptr) {
        out = stdout_text;
    }
    g_free(stdout_text);
    if (error != nullptr) g_error_free(error);
    return out;
}

// What apt believes it could install right now. Empty when apt is absent, when
// the package is not from a repository, or when the candidate is what is
// already installed.
std::string apt_candidate(std::string *installed_out) {
    const std::string policy = run_capture({"apt-cache", "policy", kPackage});
    if (policy.empty()) return {};

    std::string installed;
    std::string candidate;
    std::istringstream lines(policy);
    std::string line;
    while (std::getline(lines, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (key == "Installed") installed = value;
        else if (key == "Candidate") candidate = value;
    }
    if (installed_out != nullptr) *installed_out = installed;
    if (candidate.empty() || candidate == "(none)" || candidate == installed) return {};
    return candidate;
}

void detect_method(Check &check) {
    // dpkg first: a Debian machine with both dpkg and rpm installed is still a
    // Debian machine, and asking rpm about a .deb-installed package would
    // answer "not installed" rather than "wrong tool".
    if (!run_capture({"dpkg-query", "-W", "-f=${Version}", kPackage}).empty()) {
        check.method = Method::Apt;
        check.candidate = apt_candidate(nullptr);
        // Compared against the running binary, not against what dpkg believes
        // is installed: a from-source install over a packaged one leaves dpkg
        // describing a version that has not been on disk for months, and
        // offering to "upgrade" to something older than what is running would
        // be a downgrade wearing the wrong label.
        check.upgradable = !check.candidate.empty() &&
                           is_newer(check.candidate, current_version());
        check.command = std::string("apt-get install --only-upgrade ") + kPackage;
        return;
    }
    if (!run_capture({"rpm", "-q", kPackage}).empty()) {
        check.method = Method::Dnf;
        // dnf has no cheap offline equivalent of apt-cache policy, and
        // `dnf check-update` hits the network on its own — which this window
        // has already done once. So the repository's own idea of a candidate is
        // unknown here, and GitHub's release stands in for it.
        check.candidate = check.latest;
        check.upgradable = check.newer;
        check.command = std::string("dnf upgrade ") + kPackage;
        return;
    }
    check.method = Method::Unknown;
}

}  // namespace

std::string version_core(const std::string &version) {
    std::string core;
    for (char ch : version) {
        if ((ch >= '0' && ch <= '9') || ch == '.') core += ch;
        else break;
    }
    // Falls back to the input rather than to an empty string: a version this
    // cannot parse is still worth showing, and an empty one would read as a
    // missing installation.
    return core.empty() ? version : core;
}

std::string current_version() { return version_core(TELEBIT_VERSION); }

bool is_newer(const std::string &candidate, const std::string &current) {
    const std::vector<int> a = version_parts(candidate);
    const std::vector<int> b = version_parts(current);
    if (a.empty()) return false;
    for (std::size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const int left = i < a.size() ? a[i] : 0;
        const int right = i < b.size() ? b[i] : 0;
        if (left != right) return left > right;
    }
    return false;
}

std::string tag_from_release_json(const std::string &json) {
    const std::string key = "\"tag_name\"";
    const auto at = json.find(key);
    if (at == std::string::npos) return {};

    const auto colon = json.find(':', at + key.size());
    if (colon == std::string::npos) return {};
    const auto open = json.find('"', colon);
    if (open == std::string::npos) return {};
    const auto close = json.find('"', open + 1);
    if (close == std::string::npos) return {};

    std::string tag = json.substr(open + 1, close - open - 1);
    // Tags are published as v2.14.0; the number is what gets compared and
    // displayed.
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) tag.erase(0, 1);
    return tag;
}

const char *releases_url() { return kReleasesPage; }

Check check() {
    Check result;

    // curl_global_init() is not thread-safe and must run once before any
    // curl_easy_init(). This runs on a worker thread, and the addon in the same
    // source tree guards its own init the same way.
    static std::once_flag curl_init;
    std::call_once(curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        result.error = "Không khởi tạo được libcurl.";
        detect_method(result);
        return result;
    }

    std::string body;
    curl_slist *headers = nullptr;
    // GitHub rejects requests with no User-Agent, and the API version header is
    // what keeps tag_name from moving under a future default.
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    headers = curl_slist_append(headers, "User-Agent: telebit-setup");

    curl_easy_setopt(curl, CURLOPT_URL, kReleasesApi);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        result.error = std::string("Không kết nối được tới GitHub (") +
                       curl_easy_strerror(code) + ").";
    } else if (status == 403 || status == 429) {
        // Unauthenticated callers get 60 requests an hour per address. Saying so
        // beats "unknown error" for someone behind a shared NAT.
        result.error = "GitHub tạm thời chặn vì quá nhiều lượt kiểm tra. Thử lại sau ít phút.";
    } else if (status != 200) {
        result.error = "GitHub trả về mã " + std::to_string(status) + ".";
    } else {
        result.latest = tag_from_release_json(body);
        if (result.latest.empty()) {
            result.error = "Không đọc được số phiên bản từ GitHub.";
        } else {
            result.newer = is_newer(result.latest, current_version());
        }
    }

    detect_method(result);
    return result;
}

}  // namespace telebit::setup::update
