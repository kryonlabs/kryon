#!/bin/sh
set -eu

src=${1:?source markdown}
template=${2:?html template}
out=${3:?output html}
tmp="${out}.body"

if command -v cmark >/dev/null 2>&1; then
    cmark --unsafe "$src" > "$tmp"
else
awk '
function esc(s) {
    gsub(/&/, "\\&amp;", s)
    gsub(/</, "\\&lt;", s)
    gsub(/>/, "\\&gt;", s)
    return s
}
function inline_code(s, out, pre, code, post) {
    out = ""
    while(match(s, /`[^`]+`/)) {
        pre = substr(s, 1, RSTART - 1)
        code = substr(s, RSTART + 1, RLENGTH - 2)
        post = substr(s, RSTART + RLENGTH)
        out = out esc(pre) "<code>" esc(code) "</code>"
        s = post
    }
    return out esc(s)
}
function close_list() {
    if(in_list) {
        print "</ul>"
        in_list = 0
    }
}
function close_para() {
    if(in_para) {
        print "</p>"
        in_para = 0
    }
}
function slug(s) {
    gsub(/`/, "", s)
    gsub(/[^A-Za-z0-9]+/, "-", s)
    gsub(/^-+|-+$/, "", s)
    return tolower(s)
}
BEGIN {
    in_code = 0
    in_list = 0
    in_para = 0
}
/^```/ {
    close_para()
    close_list()
    if(in_code) {
        print "</code></pre>"
        in_code = 0
    } else {
        lang = substr($0, 4)
        printf "<pre><code"
        if(lang != "")
            printf " class=\"language-%s\"", esc(lang)
        print ">"
        in_code = 1
    }
    next
}
in_code {
    print esc($0)
    next
}
/^# / {
    close_para()
    close_list()
    text = substr($0, 3)
    printf "<h1 id=\"%s\">%s</h1>\n", slug(text), inline_code(text)
    next
}
/^## / {
    close_para()
    close_list()
    text = substr($0, 4)
    printf "<h2 id=\"%s\">%s</h2>\n", slug(text), inline_code(text)
    next
}
/^### / {
    close_para()
    close_list()
    text = substr($0, 5)
    printf "<h3 id=\"%s\">%s</h3>\n", slug(text), inline_code(text)
    next
}
/^#### / {
    close_para()
    close_list()
    text = substr($0, 6)
    printf "<h4 id=\"%s\">%s</h4>\n", slug(text), inline_code(text)
    next
}
/^- / {
    close_para()
    if(!in_list) {
        print "<ul>"
        in_list = 1
    }
    print "<li>" inline_code(substr($0, 3)) "</li>"
    next
}
/^$/ {
    close_para()
    close_list()
    next
}
{
    close_list()
    if(!in_para) {
        printf "<p>"
        in_para = 1
    } else {
        printf "\n"
    }
    printf "%s", inline_code($0)
}
END {
    close_para()
    close_list()
    if(in_code)
        print "</code></pre>"
}
' "$src" > "$tmp"
fi

awk '
FNR == NR {
    body = body $0 "\n"
    next
}
{
    p = index($0, "{{ API_BODY }}")
    if(p) {
        print substr($0, 1, p - 1) body substr($0, p + 14)
    } else {
        print
    }
}
' "$tmp" "$template" > "$out"
rm "$tmp"
