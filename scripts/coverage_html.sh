#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-${ROOT_DIR}/build/coverage/index.html}"
COVERAGE_BUILD_DIR="build/coverage-build"

if [[ "${OUT_FILE}" != /* ]]; then
  OUT_FILE="${ROOT_DIR}/${OUT_FILE}"
fi

COVERAGE_CFLAGS='-O0 -g --coverage -Iinclude -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef -std=c11'

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is not installed."
  echo "Install with: pip install gcovr"
  exit 1
fi

make clean >/dev/null
make BUILD_DIR="${COVERAGE_BUILD_DIR}" test CFLAGS="${COVERAGE_CFLAGS}" >/dev/null

mkdir -p "$(dirname "${OUT_FILE}")"
gcovr -r "${ROOT_DIR}" \
  --object-directory "${ROOT_DIR}/${COVERAGE_BUILD_DIR}/obj" \
  --filter "${ROOT_DIR}/src" \
  --html \
  --html-details \
  --output "${OUT_FILE}"

echo "Coverage HTML report written to: ${OUT_FILE}"