#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  Scripts/Run-CodeRabbit.sh [mode] [base] [extra coderabbit review args...]

Modes:
  committed    Review committed changes against base branch (default)
  last         Review only the last commit (HEAD~1..HEAD)
  uncommitted  Review current working tree (fails if >150 files)
  plain        Same as committed, but with --plain output

Examples:
  Scripts/Run-CodeRabbit.sh
  Scripts/Run-CodeRabbit.sh committed master
  Scripts/Run-CodeRabbit.sh last
  Scripts/Run-CodeRabbit.sh plain master
  Scripts/Run-CodeRabbit.sh committed master --prompt-only
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
CONFIG_FILE="$REPO_ROOT/AGENTS.md"

MODE="${1:-committed}"
BASE="${2:-master}"

if ! command -v coderabbit >/dev/null 2>&1; then
    echo "[error] coderabbit CLI is not installed or not in PATH."
    echo "Run: source ~/.bashrc && coderabbit --version"
    exit 1
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "[error] Missing config file: $CONFIG_FILE"
    exit 1
fi

cd "$REPO_ROOT"

case "$MODE" in
    committed)
        shift $(( $# >= 2 ? 2 : $# ))
        coderabbit review -t committed --base "$BASE" -c "$CONFIG_FILE" "$@"
        ;;
    last)
        shift
        coderabbit review -t committed --base-commit HEAD~1 -c "$CONFIG_FILE" "$@"
        ;;
    uncommitted)
        shift
        changed_files="$(git status --porcelain=v1 | wc -l | tr -d ' ')"
        if (( changed_files > 150 )); then
            echo "[error] Uncommitted file count is $changed_files (CodeRabbit limit: 150)."
            echo "Use: Scripts/Run-CodeRabbit.sh committed master"
            exit 2
        fi
        coderabbit review -t uncommitted -c "$CONFIG_FILE" "$@"
        ;;
    plain)
        shift $(( $# >= 2 ? 2 : $# ))
        coderabbit review -t committed --base "$BASE" --plain -c "$CONFIG_FILE" "$@"
        ;;
    *)
        echo "[error] Unknown mode: $MODE"
        usage
        exit 1
        ;;
esac
