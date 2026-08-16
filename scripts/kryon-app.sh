#!/bin/sh
# kryon-app.sh -- run targets defined in a project.kryon manifest.
#
# Usage:
#   kryon-app.sh            run the project's default_target
#   kryon-app.sh <name>     run the named target
#   kryon-app.sh list       list target names (default marked with *)
#
# project.kryon grammar (one directive per line, sh-safe values only):
#   name "Uku"
#   generated "build/kryon/generated"
#   default_target "Web"
#   target "Web" "web" "make web"
#
# The final target field is a shell command executed from the project root
# (the directory holding project.kryon). default_target falls back to the
# first declared target.

set -eu

die() {
    echo "kryon-app: $*" >&2
    exit 1
}

project_file="project.kryon"
[ -f "$project_file" ] || die "no project.kryon in $(pwd)"

# awk parser: emits "key<TAB>value" for name/default_target and
# "target<TAB>Name<TAB>command" for each target.
parse_project() {
    awk '
    {
        line = $0
        # strip comments
        sub(/#.*/, "", line)
        if (line ~ /^[[:space:]]*name[[:space:]]+"/) {
            val = line
            sub(/^[[:space:]]*name[[:space:]]+"/, "", val)
            sub(/".*$/, "", val)
            print "name\t" val
        } else if (line ~ /^[[:space:]]*default_target[[:space:]]+"/) {
            val = line
            sub(/^[[:space:]]*default_target[[:space:]]+"/, "", val)
            sub(/".*$/, "", val)
            print "default_target\t" val
        } else if (line ~ /^[[:space:]]*target[[:space:]]+"/) {
            rest = line
            sub(/^[[:space:]]*target[[:space:]]+"/, "", rest)
            name = rest
            sub(/".*$/, "", name)
            # skip the id field, keep the command field
            sub(/^[^"]*"/, "", rest)
            sub(/^[^"]*"/, "", rest)
            sub(/^[[:space:]]*"/, "", rest)
            sub(/"[[:space:]]*$/, "", rest)
            print "target\t" name "\t" rest
        }
    }' "$project_file"
}

app_name=$(parse_project | awk -F'\t' '$1 == "name" {print $2; exit}')
default_target=$(parse_project | awk -F'\t' '$1 == "default_target" {print $2; exit}')

target_names() {
    parse_project | awk -F'\t' '$1 == "target" {print $2}'
}

target_command() {
    parse_project | awk -F'\t' -v want="$1" '$1 == "target" && $2 == want {print $3; exit}'
}

[ -n "$app_name" ] || app_name="kryon app"

case "${1:-}" in
    list)
        [ -n "$default_target" ] || default_target=$(target_names | head -n 1)
        for name in $(target_names); do
            [ "$name" = "$default_target" ] && mark="*" || mark=" "
            echo "$mark $name"
        done
        ;;
    "")
        want="${default_target:-$(target_names | head -n 1)}"
        [ -n "$want" ] || die "no targets defined in $project_file"
        ;;
    *)
        want="$1"
        ;;
esac

if [ -n "${want:-}" ]; then
    command=$(target_command "$want")
    [ -n "$command" ] || die "unknown target '$want' (try: $(basename "$0") list)"
    echo "kryon-app: $app_name -> $want"
    exec sh -c "$command"
fi
