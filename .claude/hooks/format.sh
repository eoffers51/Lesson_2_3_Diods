#!/usr/bin/env bash
# PostToolUse hook: run esp-clang's clang-format on edited .c/.h files.
f=$(sed -n 's/.*"file_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)
f=${f//\\\\/\\}
case "$f" in
*.c | *.h) ;;
*) exit 0 ;;
esac
cf=$(ls /c/Espressif/tools/esp-clang/*/esp-clang/bin/clang-format.exe 2>/dev/null | tail -1)
[ -n "$cf" ] || exit 0
"$cf" -i --style=file "$f"
