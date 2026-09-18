#!/usr/bin/env bash
# 统一格式化（或检查）C++ 源码。
# 用法：
#   tools/format.sh          # 就地格式化
#   tools/format.sh --check  # 仅检查，不修改（CI 使用）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-}"

if [[ "${MODE}" == "--check" ]]; then
  CLANG_ARGS="clang-format --dry-run --Werror"
else
  CLANG_ARGS="clang-format -i"
fi

docker run --rm -v "${ROOT}":/ws -w /ws python:3.12-slim bash -lc \
  "pip install -q 'clang-format==18.1.8' && find src -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | xargs -0 -r ${CLANG_ARGS}"
