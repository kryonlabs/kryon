# Kryon Bash Completion
# Installation: source this file in your .bashrc or place in /usr/share/bash-completion/completions/

_kryon_completion() {
    local cur prev words cword
    _init_completion || return

    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Level 1: Complete main commands
    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "compile run dev debug package --help --version" -- "${cur}") )
        return 0
    fi

    # Level 2: Command-specific completion
    local command="${COMP_WORDS[1]}"
    case "${command}" in
        compile)
            _kryon_compile
            ;;
        run)
            _kryon_run
            ;;
        debug)
            _kryon_debug
            ;;
        dev|package)
            _kryon_stubs
            ;;
    esac
}

_kryon_compile() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Complete options
    case "${prev}" in
        -o|--output)
            _filedir
            return 0
            ;;
    esac

    # Complete flags
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "-o --output -v --verbose -O --optimize -d --debug -h --help" -- "${cur}") )
        return 0
    fi

    # Complete .kry files
    _filedir kry
}

_kryon_run() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Complete options with arguments
    case "${prev}" in
        -r|--renderer)
            COMPREPLY=( $(compgen -W "text raylib web" -- "${cur}") )
            return 0
            ;;
        -o|--output)
            _filedir -d
            return 0
            ;;
    esac

    # Complete flags
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "-r --renderer -o --output -d --debug -h --help" -- "${cur}") )
        return 0
    fi

    # Complete .krb and .kry files
    local IFS=$'\n'
    local krb_files=($(compgen -G "*.krb" -X "*(^)" -- "${cur}"))
    local kry_files=($(compgen -G "*.kry" -X "*(^)" -- "${cur}"))
    COMPREPLY+=("${krb_files[@]}" "${kry_files[@]}")

    # Auto-add extension if only basename given
    if [[ ${#COMPREPLY[@]} -eq 0 && -n "${cur}" ]]; then
        COMPREPLY=($(compgen -W "$(ls ${cur}*.krb 2>/dev/null) $(ls ${cur}*.kry 2>/dev/null)"))
    fi
}

_kryon_debug() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Complete flags
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "--tree --help" -- "${cur}") )
        return 0
    fi

    # Complete .krb files
    _filedir krb
}

_kryon_stubs() {
    # For dev and package commands - just show help flag
    local cur="${COMP_WORDS[COMP_CWORD]}"
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help" -- "${cur}") )
    fi
}

complete -F _kryon_completion kryon
