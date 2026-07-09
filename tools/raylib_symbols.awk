/^[[:space:]]*(RLAPI|RMAPI)[[:space:]]/ {
    line = $0
    sub(/\(.*/, "", line)
    n = split(line, parts, /[[:space:]\*]+/)
    if(n > 0 && parts[n] ~ /^[A-Za-z_][A-Za-z0-9_]*$/)
        print parts[n]
}
/^#define[[:space:]]+[A-Za-z_][A-Za-z0-9_]*/ {
    print $2
}
/^}[[:space:]]*[A-Za-z_][A-Za-z0-9_]*;/ {
    name = $2
    sub(/;.*/, "", name)
    print name
}
