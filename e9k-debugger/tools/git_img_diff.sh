#!/bin/sh
# git_img_diff FILE [REF]
# Example:
#   git_img_diff amiga/example/176.png
#   git_img_diff amiga/example/176.png HEAD~1

set -e

f="$1"
ref="${2:-HEAD}"

# sanitize filename for /tmp
stem=$(echo "$f" | sed 's#[/ ]#_#g')

a="/tmp/${stem}.ref.png"
b="/tmp/${stem}.work.png"
c="/tmp/${stem}.compare.png"
t="/tmp/${stem}.triple.png"

# extract ref image (force png decode avoids IM mis-detect)
git show "$ref:$f" > "$a"

# normalize work image to png
magick "$f" "$b"

# diff + triple (compare returns non-zero when different)
magick compare -metric AE "$a" "$b" "$c" >/dev/null 2>&1 || true
magick montage "$a" "$b" "$c" -tile 3x1 -geometry +0+0 "$t"

# auto-open
if command -v open >/dev/null 2>&1; then
  open "$t" >/dev/null 2>&1
elif command -v xdg-open >/dev/null 2>&1; then
  xdg-open "$t" >/dev/null 2>&1
fi

echo "$t"
