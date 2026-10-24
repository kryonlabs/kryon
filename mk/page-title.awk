# Gives a page shell the program's name: the text of its first <title>
# element becomes KRYON_PAGE_TITLE, escaped for HTML.
BEGIN {
    title = ENVIRON["KRYON_PAGE_TITLE"]
    gsub(/&/, "\\&amp;", title)
    gsub(/</, "\\&lt;", title)
    gsub(/>/, "\\&gt;", title)
}
!done && (start = index($0, "<title>")) > 0 && (finish = index($0, "</title>")) > start {
    $0 = substr($0, 1, start + 6) title substr($0, finish)
    done = 1
}
{ print }
