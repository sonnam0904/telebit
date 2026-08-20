#!/usr/bin/env bash
# Check the Telex core against real dictionaries, from both directions.
#
# Vietnamese (round-trip): for every syllable, Unicode -> the Telex keystrokes
# that type it (to_telex.py) -> telex_to_unicode() -> compare with the original.
# Anything that does not come back is a missing rime, a misplaced tone, or a
# keystroke sequence the engine reads differently than a human would.
# Both tone styles are run, and a syllable only counts as a failure when it fails
# under BOTH — the dictionaries mix "hoà" and "hủy", so a single style would
# report hundreds of false positives.
#
# English (passthrough): every word must come back byte-identical. This is the
# other side of the same coin — widening the Vietnamese rime table makes more
# keystroke sequences convertible, which can start eating English words.
#
# Both results are diffed against the committed baselines in baseline/, word by
# word rather than by count, so the run names exactly which words a change broke,
# which it fixed, and which now come out differently. Exit 1 on anything broken
# or changed; improvements pass but ask you to record them.
#
#   ./run.sh                     # check against the baselines
#   ./run.sh --update-baseline   # record the current state as the new baselines
#
# Both corpora are committed under dict/ at the repo root, so a run needs no
# network, no system packages, and produces the same numbers on every machine —
# which is what makes the baselines comparable at all. See dict/README.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="${ROOT}/scripts/dict-roundtrip"
WORK="${TELEBIT_ROUNDTRIP_WORK:-${HERE}/.work}"
BASELINE="${HERE}/baseline"
UPDATE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --update-baseline) UPDATE=1; shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

mkdir -p "${WORK}"

# --- corpus ----------------------------------------------------------------
# Both lists are committed, and pinning them is the point: the baselines are a
# word-for-word record, so a corpus that differs between machines would report
# other people's dictionary version as your regression. Refuse to guess at a
# system copy — a silently different corpus is worse than no run at all.
DICT="${ROOT}/dict"
vietnamese="${DICT}/vietnamese"
english="${DICT}/american-english"

for f in "${vietnamese}" "${english}"; do
  if [[ ! -r "${f}" ]]; then
    echo "missing corpus: ${f}" >&2
    echo "dict/ is committed to the repo; see dict/README.md to rebuild it" >&2
    exit 3
  fi
done

# hunspell-vi only, on purpose. It is an edited syllable list, so a failure in it
# means something. Large scraped wordlists (Viet74K and friends) are not usable
# as evidence here: they mix in dialect variants, misspelled reduplications and
# proper nouns, so "this syllable fails" cannot be told apart from "this entry
# was never a Vietnamese syllable". Chasing those produced a list of rimes —
# uêu, oây, uăng — that do not exist; the real spellings (khều, nguẩy, khuấy)
# already worked.
#
# Drop anything with a non-letter; the engine works on one syllable at a time.
grep -vP '[^\p{L}]' "${vietnamese}" | sort -u > "${WORK}/syllables.txt"
python3 "${HERE}/to_telex.py" < "${WORK}/syllables.txt" > "${WORK}/pairs.tsv"

# --- build & run -----------------------------------------------------------
cmake --build "${ROOT}/build" --target telebit_telex_core >/dev/null
for tool in roundtrip passthrough; do
  g++ -std=c++17 -I"${ROOT}" "${HERE}/${tool}.cpp" \
      "${ROOT}/build/libtelebit_telex_core.a" -o "${WORK}/${tool}"
done

# The tools report "total=N failed=N" on stderr; grab it so report.py prints the
# very numbers the binaries measured instead of recounting lines. A tool that
# died before printing its summary must stop the run here — passing an empty
# string on would surface as a Python traceback three steps later.
counts() {
  local n
  n="$(sed -n "s/.*${2}=\([0-9]\+\).*/\1/p" "${1}" | tail -1)"
  if [[ -z "${n}" ]]; then
    echo "no '${2}=' in ${1} — the tool failed before reporting; see that file" >&2
    exit 4
  fi
  printf '%s' "${n}"
}

"${WORK}/roundtrip" "${WORK}/pairs.tsv"        > "${WORK}/fail-classic.tsv" 2> "${WORK}/classic.log"
"${WORK}/roundtrip" "${WORK}/pairs.tsv" modern > "${WORK}/fail-modern.tsv"  2> "${WORK}/modern.log"

# A syllable only counts when it fails under BOTH styles; the details come from
# the modern run, which is the style hunspell-vi mostly follows.
cut -f1 "${WORK}/fail-classic.tsv" | sort > "${WORK}/.classic-names"
cut -f1 "${WORK}/fail-modern.tsv"  | sort > "${WORK}/.modern-names"
comm -12 "${WORK}/.classic-names" "${WORK}/.modern-names" > "${WORK}/.both-names"
awk -F'\t' 'NR==FNR { keep[$0]; next } $1 in keep' \
    "${WORK}/.both-names" "${WORK}/fail-modern.tsv" | sort > "${WORK}/vietnamese.tsv"

"${WORK}/passthrough" "${english}" 2> "${WORK}/english.log" | sort > "${WORK}/english.tsv"

# --- baseline --------------------------------------------------------------
if (( UPDATE )); then
  mkdir -p "${BASELINE}"
  cp "${WORK}/vietnamese.tsv" "${BASELINE}/vietnamese.tsv"
  cp "${WORK}/english.tsv"    "${BASELINE}/english.tsv"
  printf '\n  ✔ Đã chốt baseline mới: %s âm tiết Việt, %s từ Anh\n' \
      "$(wc -l < "${BASELINE}/vietnamese.tsv")" "$(wc -l < "${BASELINE}/english.tsv")" >&2
  printf '    Nhớ commit %s cùng trong PR.\n\n' "${BASELINE#"${ROOT}"/}" >&2
  exit 0
fi

status=0
python3 "${HERE}/report.py" \
    "${vietnamese#"${ROOT}"/}" "$(wc -l < "${WORK}/pairs.tsv")" \
    "$(counts "${WORK}/classic.log" failed)" "$(counts "${WORK}/modern.log" failed)" \
    "${english#"${ROOT}"/}" "$(counts "${WORK}/english.log" total)" \
    "$(counts "${WORK}/english.log" mangled)" \
    "${BASELINE}/vietnamese.tsv" "${WORK}/vietnamese.tsv" \
    "${BASELINE}/english.tsv"    "${WORK}/english.tsv" || status=$?

exit "${status}"
