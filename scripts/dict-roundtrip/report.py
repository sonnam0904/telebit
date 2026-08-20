#!/usr/bin/env python3
"""Render the dictionary-check report and compare against the committed baseline.

Replaces the old compare.py. Everything the user sees comes from here so the
tables line up; run.sh only collects data.

Comparing whole rows, not counts, is the point: a change that fixes three
syllables and breaks three others leaves the count alone. Three kinds of
difference exist —

  mới hỏng   (NEW)      failing now, absent from the baseline   -> exit 1
  đổi kết quả (CHANGED) failing in both, different output       -> exit 1
  đã sửa     (FIXED)    in the baseline, passing now            -> exit 0

FIXED does not fail the run but does mean the baseline is stale.
"""

import sys
import unicodedata

# Same box glyphs and status marks as `telebit doctor`, so the two tools read as
# one family.
OK, WARN, INFO = "✔", "!", "·"
MAX_ROWS = 25  # per group; the rest stay in the .work file, and we say so


def width(s):
    """Display width. Vietnamese is single-width, but a decomposed tone mark
    would otherwise be counted as its own column."""
    return sum(0 if unicodedata.combining(c) else 1 for c in s)


def pad(s, n, right=False):
    fill = " " * max(0, n - width(s))
    return fill + s if right else s + fill


def num(n):
    """6631 -> 6.631, the Vietnamese thousands separator."""
    return f"{n:,}".replace(",", ".")


def table(headers, rows, aligns=None, out=sys.stderr):
    """headers: list[str]; rows: list[list[str]]; aligns: 'l'/'r' per column."""
    cols = len(headers)
    aligns = aligns or ["l"] * cols
    w = [width(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            w[i] = max(w[i], width(cell))

    def line(left, mid, right):
        return left + mid.join("─" * (n + 2) for n in w) + right

    def render(cells):
        parts = [
            " " + pad(c, w[i], right=(aligns[i] == "r")) + " "
            for i, c in enumerate(cells)
        ]
        return "│" + "│".join(parts) + "│"

    print(line("┌", "┬", "┐"), file=out)
    print(render(headers), file=out)
    print(line("├", "┼", "┤"), file=out)
    for row in rows:
        print(render(row), file=out)
    print(line("└", "┴", "┘"), file=out)


def heading(text, out=sys.stderr):
    print(f"\n  {text}", file=out)


def load(path):
    """Read a TSV failure list into {word: rest-of-line}."""
    rows = {}
    try:
        with open(path, encoding="utf-8") as f:
            for raw in f:
                raw = raw.rstrip("\n")
                if not raw:
                    continue
                word, _, detail = raw.partition("\t")
                rows[word] = detail
    except FileNotFoundError:
        return None
    return rows


def detail_cells(detail):
    """A failure detail is 'keys\tgot' (Vietnamese) or just 'got' (English)."""
    parts = detail.split("\t")
    return parts if len(parts) == 2 else ["", parts[0] if parts else ""]


class Side:
    """One language: its counts, its baseline diff, its verdict."""

    def __init__(self, label, baseline_path, current_path, measured):
        self.label = label
        self.baseline_path = baseline_path
        self.current_path = current_path
        self.baseline = load(baseline_path)
        self.current = load(current_path)
        self.missing_baseline = self.baseline is None
        # "Ran and found nothing wrong" and "never ran" both leave an empty
        # results file, so the count of words actually fed through the tool is
        # what separates them. Without this a crashed roundtrip/passthrough
        # would read as a clean run and turn the gate green.
        self.no_results = self.current is None or measured == 0

        self.new = self.changed = self.fixed = []
        if self.missing_baseline or self.no_results:
            return

        self.new = sorted(w for w in self.current if w not in self.baseline)
        self.fixed = sorted(w for w in self.baseline if w not in self.current)
        self.changed = sorted(
            w for w in self.current
            if w in self.baseline and self.current[w] != self.baseline[w]
        )

    @property
    def regressed(self):
        return bool(self.new or self.changed)

    def mark(self):
        if self.missing_baseline or self.no_results or self.regressed:
            return WARN
        if self.fixed:
            return INFO
        return OK

    def verdict(self):
        if self.missing_baseline:
            return "chưa có baseline"
        if self.no_results:
            return "KHÔNG CÓ KẾT QUẢ"
        if self.regressed:
            return "CÓ HỒI QUY"
        if self.fixed:
            return "chỉ có cải thiện"
        return "khớp baseline"

    def groups(self):
        """Each group names where its details live, so the row source and the
        "xem đầy đủ ở" pointer can never disagree — words in "Đã sửa được"
        exist only in the baseline, by definition not in the current results."""
        return [
            ("Mới hỏng", WARN, self.new, self.current, self.current_path),
            ("Đổi kết quả", WARN, self.changed, self.current, self.current_path),
            ("Đã sửa được", OK, self.fixed, self.baseline, self.baseline_path),
        ]

    def print_details(self):
        for title, glyph, words, source, source_path in self.groups():
            if not words:
                continue
            heading(f"{glyph} {self.label} · {title} ({num(len(words))} từ)")
            rows = []
            for word in words[:MAX_ROWS]:
                keys, got = detail_cells(source[word])
                if title == "Đổi kết quả":
                    _, was = detail_cells(self.baseline[word])
                    got = f"{got}   (baseline: {was})"
                elif title == "Đã sửa được":
                    got = "giờ gõ đúng"
                rows.append([word, keys, got])
            # English words are typed as-is, so there is no separate key column.
            if all(not r[1] for r in rows):
                table(["Từ", "Engine cho ra"], [[r[0], r[2]] for r in rows])
            else:
                table(["Từ", "Gõ bằng phím", "Engine cho ra"], rows)
            if len(words) > MAX_ROWS:
                print(
                    f"  … và {num(len(words) - MAX_ROWS)} từ nữa — "
                    f"xem đầy đủ ở {source_path}",
                    file=sys.stderr,
                )


def count_lines(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return sum(1 for _ in f)
    except OSError:
        return 0


def main():
    if len(sys.argv) != 12:
        print(
            "usage: report.py <vi-corpus> <vi-total> <vi-classic-failed> "
            "<vi-modern-failed> <en-corpus> <en-total> <en-mangled> "
            "<baseline-vi> <current-vi> <baseline-en> <current-en>",
            file=sys.stderr,
        )
        return 2

    (vi_corpus, vi_total, vi_classic, vi_modern, en_corpus, en_total, en_mangled,
     base_vi, cur_vi, base_en, cur_en) = sys.argv[1:]
    try:
        vi_total, vi_classic, vi_modern = int(vi_total), int(vi_classic), int(vi_modern)
        en_total, en_mangled = int(en_total), int(en_mangled)
    except ValueError:
        # run.sh scrapes these off the tools' stderr; an empty one means a tool
        # died without printing its summary. Say that, rather than tracebacking.
        print(f"  {WARN} Không đọc được số liệu từ roundtrip/passthrough — "
              "một trong hai tool đã lỗi. Xem .work/*.log", file=sys.stderr)
        return 2

    vi = Side("Tiếng Việt", base_vi, cur_vi, vi_total)
    en = Side("Tiếng Anh", base_en, cur_en, en_total)
    vi_both = len(vi.current or {})

    # The corpus files hold more than what each check looks at: the Vietnamese
    # side drops entries with non-letters, the English side only takes plain
    # lowercase words. Showing both numbers keeps "Số mục" from reading as the
    # dictionary's size when it is really the filtered subset.
    heading("Ngữ liệu")
    table(
        ["Bộ từ điển", "Số mục", "Đưa vào kiểm"],
        [
            [vi_corpus, num(count_lines(vi_corpus)), num(vi_total)],
            [en_corpus, num(count_lines(en_corpus)), num(en_total)],
        ],
        aligns=["l", "r", "r"],
    )

    heading("Kết quả thô")
    table(
        ["Phép kiểm", "Tổng", "Đạt", "Lỗi"],
        [
            ["Tiếng Việt · bỏ dấu kiểu cũ", num(vi_total), num(vi_total - vi_classic), num(vi_classic)],
            ["Tiếng Việt · bỏ dấu kiểu mới", num(vi_total), num(vi_total - vi_modern), num(vi_modern)],
            ["Tiếng Việt · hỏng ở CẢ HAI kiểu", "—", "—", num(vi_both)],
            ["Tiếng Anh · giữ nguyên chữ", num(en_total), num(en_total - en_mangled), num(en_mangled)],
        ],
        aligns=["l", "r", "r", "r"],
    )
    print(
        "  Chỉ dòng “CẢ HAI kiểu” mới là lỗi thật — từ điển trộn lẫn hai quy ước bỏ dấu.",
        file=sys.stderr,
    )

    heading("So với baseline")
    table(
        ["", "Ngôn ngữ", "Mới hỏng", "Đổi kết quả", "Đã sửa", "Kết luận"],
        [
            [s.mark(), s.label, num(len(s.new)), num(len(s.changed)), num(len(s.fixed)), s.verdict()]
            for s in (vi, en)
        ],
        aligns=["l", "l", "r", "r", "r", "l"],
    )

    for side in (vi, en):
        side.print_details()

    print(file=sys.stderr)
    if vi.no_results or en.no_results:
        side = vi if vi.no_results else en
        print(f"  {WARN} {side.label}: không có kết quả nào ({side.current_path} rỗng hoặc thiếu).",
              file=sys.stderr)
        print("     Lần chạy này không kiểm được gì — KHÔNG coi là đạt. Xem .work/*.log.",
              file=sys.stderr)
        return 2
    if vi.missing_baseline or en.missing_baseline:
        print(f"  {WARN} Chưa có baseline — chạy: ./scripts/dict-roundtrip/run.sh --update-baseline",
              file=sys.stderr)
        return 2
    if vi.regressed or en.regressed:
        print(f"  {WARN} CÓ HỒI QUY. Hãy sửa, hoặc nếu đây là đánh đổi có chủ ý thì giải thích",
              file=sys.stderr)
        print("     trong mô tả PR rồi chạy: ./scripts/dict-roundtrip/run.sh --update-baseline",
              file=sys.stderr)
        return 1
    if vi.fixed or en.fixed:
        print(f"  {INFO} Chỉ có cải thiện. Chốt lại bằng: ./scripts/dict-roundtrip/run.sh --update-baseline",
              file=sys.stderr)
        return 0
    print(f"  {OK} Không có thay đổi nào so với baseline.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
