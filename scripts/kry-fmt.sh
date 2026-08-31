#!/bin/sh
set -eu

usage()
{
    cat <<'USAGE'
usage: kryon fmt [--check] file.kry [...]

Formats Kry source with stable indentation and simple spacing cleanup.
USAGE
}

check=0
if [ "${1:-}" = "--check" ]; then
    check=1
    shift
fi

[ $# -gt 0 ] || { usage >&2; exit 2; }

status=0
for file in "$@"; do
    [ -f "$file" ] || { printf 'kryon fmt: not found: %s\n' "$file" >&2; status=1; continue; }
    tmp=${TMPDIR:-/tmp}/kry-fmt.$$.tmp
    awk '
    function trim(s) {
        sub(/^[ \t\r\n]+/, "", s)
        sub(/[ \t\r\n]+$/, "", s)
        return s
    }
    function count_char(s, ch,    i,n,c,q,esc) {
        n = 0; q = ""; esc = 0
        for(i = 1; i <= length(s); i++) {
            c = substr(s, i, 1)
            if(q != "") {
                if(esc) esc = 0
                else if(c == "\\") esc = 1
                else if(c == q) q = ""
            } else if(c == "\"" || c == "'\''") {
                q = c
            } else if(c == ch) {
                n++
            }
        }
        return n
    }
    function fmt_spacing(s,    out,i,c,n,q,esc,nextc,prevc) {
        out = ""; q = ""; esc = 0; n = length(s)
        for(i = 1; i <= n; i++) {
            c = substr(s, i, 1)
            nextc = i < n ? substr(s, i + 1, 1) : ""
            prevc = i > 1 ? substr(s, i - 1, 1) : ""
            if(q != "") {
                out = out c
                if(esc) esc = 0
                else if(c == "\\") esc = 1
                else if(c == q) q = ""
            } else if(c == "\"" || c == "'\''") {
                q = c; out = out c
            } else if(c == ":" && nextc == ":") {
                sub(/[ \t]+$/, "", out)
                out = out " :: "
                i++
                while(i < n && substr(s, i + 1, 1) ~ /[ \t]/) i++
            } else if(c == "=" && prevc !~ /[!<>=:+\-*\/%&|^]/ && nextc != "=") {
                sub(/[ \t]+$/, "", out)
                out = out " = "
                while(i < n && substr(s, i + 1, 1) ~ /[ \t]/) i++
            } else {
                out = out c
            }
        }
        return trim(out)
    }
    {
        raw = $0
        line = fmt_spacing(trim(raw))
        if(line == "") {
            print ""
            next
        }
        leading_close = substr(line, 1, 1) == "}"
        if(leading_close && indent > 0)
            indent--
        for(i = 0; i < indent; i++)
            printf "    "
        print line
        opens = count_char(line, "{")
        closes = count_char(line, "}")
        delta = opens - closes
        if(leading_close)
            delta++
        indent += delta
        if(indent < 0)
            indent = 0
    }' "$file" > "$tmp"
    if [ "$check" -eq 1 ]; then
        if ! cmp -s "$file" "$tmp"; then
            printf 'kryon fmt: would reformat %s\n' "$file" >&2
            status=1
        fi
        rm -f "$tmp"
    else
        mv "$tmp" "$file"
    fi
done

exit "$status"
