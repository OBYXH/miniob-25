#!/usr/bin/env bash
set -euo pipefail

# 仅从同目录下的 test.txt 读取测试用例（每行一个，支持注释 # 和空行）
FILE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/testcases.txt"

if [[ ! -f "$FILE" ]]; then
  echo "Test list file not found: $FILE" >&2
  echo "Usage: 将要运行的测试名按行写入 $FILE（支持注释以 # 开头），然后直接运行本脚本。" >&2
  exit 2
fi

mapfile -t lines < <(grep -v '^\s*$\|^\s*#' "$FILE" || true)
TESTS="$(IFS=,; echo "${lines[*]}")"

echo "Running tests from file: $FILE"
echo "Tests: $TESTS"

python3 ./miniob_test.py --test-cases="$TESTS"