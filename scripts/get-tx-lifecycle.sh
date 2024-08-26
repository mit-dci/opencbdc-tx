#!/usr/bin/env bash

LOGDIR='.'
TXID=

IFS='' read -r -d '' usage <<'EOF'
Usage: %s [options]

Options:
  -h, --help              print this help and exit
  -d, --log-dir=PATH      PATH where the log-files are located
                          (defaults to '.' if not specified)
  -t, --tx-id=ID          ID of the transaction to trace
EOF

_help=
if [[ $# -eq 0 ]]; then
    _help=1
fi

_err=0
while [[ $# -gt 0 ]]; do
    optarg=
    shft_cnt=1
    if [[ "$1" =~ [=] ]]; then
        optarg="${1#*=}"
    elif [[ "$1" =~ ^-- && $# -gt 1 && ! "$2" =~ ^- ]]; then
        optarg="$2"
        shft_cnt=2
    elif [[ "$1" =~ ^-[^-] && $# -gt 1 && ! "$2" =~ ^- ]]; then
        optarg="$2"
        shft_cnt=2
    elif [[ "$1" =~ ^-[^-] ]]; then
        optarg="${1/??/}"
    fi

    case "$1" in
        -d*|--log-dir*) LOGDIR="${optarg:-$LOGDIR}"; shift "$shft_cnt";;
        -t*|--tx-id*)
            if [[ ! "$optarg" ]]; then
                printf '`--tx-id` requires an argument\n'
                _help=1; _err=1;
                break
            else
                TXID="$optarg"
                shift "$shft_cnt"
            fi;;
        -h|--help) _help=1; shift "$shft_cnt";;
        *)
            printf 'Unrecognized option: %s\n' "$1"
            _help=1; _err=1;
            break;;
    esac
done

if [[ -n "$_help" ]]; then
    printf "$usage" "$(basename $0)"
    exit "$_err"
fi

pushd "$LOGDIR" &>/dev/null

all_lines=$(grep -n "$TXID.*at" *.{log,txt})
sorted_lines=$(printf "%s\n" "$all_lines" | awk -F ' at ' '{ print $2, $0 }' | sort | sed 's/^[^ ]* //')

printf "%s\n" "$sorted_lines"

fst=$(printf "%s\n" "$sorted_lines" | head -n 1)
lst=$(printf "%s\n" "$sorted_lines" | tail -n 1)
if [[ $(printf "%s\n" "$fst" | cut -d':' -f1) != $(printf "%s\n" "$lst" | cut -d':' -f1) ]]; then
    printf 'First and last message for %s are in different logs!\n' "$TXID"
fi

start=$(printf "%s\n" "$fst" | awk -F ' at ' '{ print $2 }')
end=$(printf "%s\n" "$lst" | awk -F ' at ' '{ print $2 }')

printf "total elapsed time (assuming all clocks synchronized): %d ticks\n" "$(( end - start ))"

popd &>/dev/null
