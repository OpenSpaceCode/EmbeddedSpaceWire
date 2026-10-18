#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-${ROOT_DIR}/build/coverage/index.html}"
COVERAGE_BUILD_DIR="build/coverage-build"

if [[ "${OUT_FILE}" != /* ]]; then
  OUT_FILE="${ROOT_DIR}/${OUT_FILE}"
fi

# Instrumentation is the only thing that differs from the normal build; the C standard,
# include paths and warning set (including -Werror) come from the Makefile.
COVERAGE_OPT='-O0 -g --coverage'

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is not installed."
  echo "Install with: pip install gcovr"
  exit 1
fi

make clean >/dev/null
make BUILD_DIR="${COVERAGE_BUILD_DIR}" test OPT="${COVERAGE_OPT}" >/dev/null

mkdir -p "$(dirname "${OUT_FILE}")"

# Emit the HTML report and a text summary (line + branch) in a single gcovr pass, so
# the console output is not duplicated, and fail below 100% line or branch coverage.
# gcovr writes the report before it checks the thresholds, so the report exists either
# way. Its chatty "(INFO)" progress lines are filtered from stderr; warnings and errors
# still pass through.
echo "Coverage:"
status=0
gcovr -r "${ROOT_DIR}" \
  --object-directory "${ROOT_DIR}/${COVERAGE_BUILD_DIR}/obj" \
  --filter "${ROOT_DIR}/src" \
  --html-details --output "${OUT_FILE}" \
  --txt - --txt-summary \
  --fail-under-line 100 \
  --fail-under-branch 100 \
  2> >(grep -v '^(INFO)' >&2) || status=$?

echo "Coverage HTML report written to: ${OUT_FILE}"
if [[ ${status} -ne 0 ]]; then
  echo "Error: coverage is below 100% line or branch (gcovr exit ${status})." >&2
fi
exit "${status}"
