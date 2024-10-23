#!/usr/bin/env bash

ROOT="$(cd "$(dirname "$0")"/.. && pwd)"

COLOR="auto"
SEVERITY="warning"
# exclude - waive these diagnostic codes at all severity levels
# include - scan for these diagnostic codes at all severity levels
# includes take precedence over excludes
EXCLUDE_CODES="SC1091"
INCLUDE_CODES="SC2086"
# becomes comma separated string for shellcheck input later

exit_on_error() {
    echo; echo -e "${RED}[ERROR]${RST_COLOR} $1"
    echo; echo "Exiting..."; echo
    exit 1
}

check_shellcheck_install() {
    if ! command -v shellcheck &>/dev/null; then
        exit_on_error "shellcheck is not installed.\n\n\
        Run '# ./scripts/install-build-tools.sh' to install shellcheck."
    fi
}

exit_bad_arg() {
    if [[ -z "$1" ]]; then
        exit_on_error "No argument passed to exit_bad_arg function"
    fi
    show_usage
    exit_on_error "Invalid argument: $1"
}

show_usage() {
cat << EOF
Usage: $0 [options]

Options:
    -h, --help               print this help and exit
    -C, --color              colorize the output, default is 'auto'
    -e, --exclude-code       exclude specific error code, can have multiple, enter one at a time
    -i, --include-code       include specific error code, can have multiple, enter one at a time
    -S, --severity=LEVEL     set severity level (style, info, warning, error), default is 'warning'

Usage: $ ./scripts/shellcheck.sh [-C|--color=MODE] [-S|--severity=LEVEL] [-e|--exclude-code=CODE] [-i|--include-code=CODE]

example: $ ./scripts/shellcheck.sh -C auto -S info -e SC2059 -e SC2317 -i SC2034

EOF
}

parse_cli_args() {
    echo
    while [[ $# -gt 0 ]]; do
        optarg=
        shft_cnt=1
        # if -- is passed then stop parsing
        if [[ "$1" = '--' ]]; then
            break
        # --option=value
        elif [[ "$1" =~ ^-- && ! "$1" =~ ^--$ ]]; then
            optarg="${1#*=}"; shft_cnt=1
        # -o=value
        elif [[ "$1" =~ ^-- && $# -gt 1 && ! "$2" =~ ^- ]]; then
            optarg="$2"; shft_cnt=2
        # -o value
        elif [[ "$1" =~ ^-[^-] && $# -gt 1 && ! "$2" =~ ^- ]]; then
            optarg="$2"; shft_cnt=2
        # -o
        elif [[ "$1" =~ ^-[^-] ]]; then
            optarg="${1/??/}"
        fi

        case "$1" in
            -S*|--severity*)
                case "${optarg}" in
                    style|info|warning|error)
                        SEVERITY="${optarg}" ;;
                    *)
                        exit_bad_arg "$optarg" ;;
                esac
                shift "$shft_cnt" ;;
            -C*|--color*)
                case "${optarg}" in
                    always|auto|never)
                        COLOR="${optarg}" ;;
                    *)
                        exit_bad_arg "$optarg" ;;
                esac
                shift "$shft_cnt" ;;
            -e*|--exclude-code*)
                # valid if matching format SC1000-SC9999
                if [[ "${optarg}" =~ ^SC[0-9]{4}$ ]]; then
                    # if empty, fill with error code, else append comma and new code
                    if [[ -z "$EXCLUDE_CODES" ]]; then
                        EXCLUDE_CODES+="${optarg}"
                    # don't add duplicate exclude code to excludes
                    elif [[ "${EXCLUDE_CODES[*]}" =~ $optarg ]]; then
                        exit_on_error "Duplicate error code: $optarg"
                    else
                        EXCLUDE_CODES+=",${optarg}"
                    fi
                else
                    exit_bad_arg "$optarg"
                fi
                shift "$shft_cnt" ;;
            -i*|--include-code*)
                if [[ "${optarg}" =~ ^SC[0-9]{4}$ ]]; then
                    # if variable INCLUDE_CODES is empty, fill with error code
                    if [[ -z "$INCLUDE_CODES" ]]; then
                        INCLUDE_CODES+="${optarg}"
                    # includes override excludes if the same code is in both categories
                    elif [[ "${INCLUDE_CODES[*]}" =~ $optarg ]]; then
                        exit_on_error "Duplicate error code: $optarg"
                    else
                        INCLUDE_CODES+=",${optarg}"
                    fi
                else
                    exit_bad_arg "$optarg"
                fi
                shift "$shft_cnt" ;;
            -h|--help)
                echo; echo "Command line arguments: $0 $*"; echo
                show_usage
                exit 0 ;;
            *)
                exit_bad_arg "$optarg" ;;
        esac
    done
}

get_num_cores() {
    local CORE_COUNT=1
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        CORE_COUNT=$(grep -c ^processor /proc/cpuinfo)
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        CORE_COUNT=$(sysctl -n hw.ncpu)
    fi
    printf "%d\n" "$CORE_COUNT"
}

show_shellcheck_args() {
    echo -e "Running shellcheck with severity level '${CYAN}${SEVERITY}${RST_COLOR}'"
    if [[ -n "$EXCLUDE_CODES" ]]; then
        echo
        echo -e "Excluding error codes: '${YELLOW}${EXCLUDE_CODES}${RST_COLOR}'"
    fi
    if [[ -n "$INCLUDE_CODES" ]]; then
        echo
        echo -e "Including error codes: '${YELLOW}${INCLUDE_CODES}${RST_COLOR}'"
    fi
    echo
}

run_shellcheck() {
    # two runs of shellcheck used since shellcheck doesn't allow for include codes
    # to be checked at all levels while using a severity level for all other codes
    #
    # 1. include codes only all severity levels (if non-empty)
    # 2. exclude codes with the include codes added at the entered severity level
    RUN_1_STATUS=0
    RUN_2_STATUS=0

    # this prevents duplicate scanning of the include codes on the excludes run
    EXCLUDES_ALL="$EXCLUDE_CODES"
    if [[ -n "$INCLUDE_CODES" && -n "$EXCLUDE_CODES" ]]; then
        EXCLUDES_ALL+=",${INCLUDE_CODES}"
    elif [[ -n "$INCLUDE_CODES" ]]; then
        EXCLUDES_ALL="${INCLUDE_CODES}"
    fi

    if command -v git &>/dev/null; then
        SHELL_SCRIPTS=$(git ls-files '*.sh')
    else
        # exclude 3rdparty scripts by specifying the scripts directory
        SHELL_SCRIPTS=$(find "$ROOT"/scripts -name '*.sh')
    fi

    # scan for only include codes, at all severity levels
    if [[ -n "$INCLUDE_CODES" ]]; then
        xargs -n 1 -P "$NUM_CORES" shellcheck -x -s bash -C"$COLOR" --include="$INCLUDE_CODES" -S "style" <<< "$SHELL_SCRIPTS"
        RUN_1_STATUS="$?"
    fi

    # blanket severity level scan, omitting any include codes (to not repeat what shellcheck just scanned for)
    xargs -n 1 -P "$NUM_CORES" shellcheck -x -s bash -C"$COLOR" --exclude="$EXCLUDES_ALL" -S "$SEVERITY" <<< "$SHELL_SCRIPTS"
    RUN_2_STATUS="$?"

    if [[ "$RUN_1_STATUS" -eq 0 && "$RUN_2_STATUS" -eq 0 ]]; then
        echo -e "${GREEN}[PASS]${RST_COLOR} Shellcheck did not detect violations"
        return 0
    else
        echo -e "${RED}[FAIL]${RST_COLOR} Shellcheck found violations"
        return 1
    fi
}

print_final_message() {
    if [[ "$#" -ne 1 ]] || [[ "$1" -ne 0 && "$1" -ne 1 ]]; then
        exit_on_error "This function needs a decimal status code as input"
    fi

    EXIT_STATUS="$1"
    # if no errors found given our parameters, then shellcheck passed
    if [[ "$EXIT_STATUS" -eq 0 ]]; then
        echo -e "${GREEN}[PASS]${RST_COLOR} Shellcheck did not detect violations"
        echo; echo -e "${GREEN}Shellcheck passed.${RST_COLOR}"; echo
    else
        echo -e "${RED}[FAIL]${RST_COLOR} Shellcheck found unexcused violations"
        echo; echo -e "${RED}Shellcheck failed. Please fix the issues and try again.${RST_COLOR}"
    fi
    exit "$EXIT_STATUS"
}

main() {
    check_shellcheck_install
    parse_cli_args "$@"

    if [[ "$COLOR" != "never" ]]; then
        RED="\e[31m"
        GREEN="\e[32m"
        YELLOW="\e[33m"
        CYAN="\e[36m"
        RST_COLOR="\e[0m"
    else
        RED=""
        GREEN=""
        YELLOW=""
        CYAN=""
        RST_COLOR=""
    fi

    NUM_CORES=$(get_num_cores)
    show_shellcheck_args

    run_shellcheck
    STATUS="$?"

    print_final_message "$STATUS"
}

main "$@"
