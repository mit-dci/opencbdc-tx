#!/usr/bin/env bash

RR='rr record --'
GDB='gdb -ex run --args'
VALGRIND='valgrind --leak-check=full'
DBG="${DBG:-$GDB}"

# runs for DURATION seconds
# if DURATION is set to inf/infinity, run indefinitely
DURATION=30
CWD=$(pwd)
COMMIT=$(git rev-parse --short HEAD)
TL=$(git rev-parse --show-toplevel)
RT="${TL:-$CWD}"
BLD="$RT"/build
SEEDDIR="$BLD"/preseeds
TESTDIR="$BLD"/test-$(date +"%s")

IFS='' read -r -d '' usage <<'EOF'
Usage: %s [options]

Options:
  -h, --help              print this help and exit
  -c, --config=PATH       use PATH as the test configuration
  -s, --samples=TIME      run test for TIME seconds (defaults to 30)
                          (or indefinitely if set to inf, or infinity)

Cleanup:
  --clean                 delete all previous test and preseed artifacts
  --clean-tests           delete all previous test artifacts
  --clean-preseeds        delete all previous preseed artifacts

Debugging Options:
  --debug                 run each component under a debugger
                          (likely requires a Debug build to be useful)
  --profile               run each component under perf
                          (likely requires a Profiling build to be useful)
  --leak-check            run each component under valgrind

  -d, --debugger=CMD      specify the debugger CMD
                          ("gdb" and "rr" are short-cuts for sensible defaults
                          for those debuggers)

Note: --debug, --profile, and --leak-check are mutually-exclusive
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
        -s*|--samples*) DURATION="${optarg:-$DURATION}"; shift "$shft_cnt";;
        --leak-check)  RECORD=debug; DBG="$VALGRIND"; shift "$shft_cnt";;
        --debug)       RECORD=debug; shift "$shft_cnt";;
        --profile)     RECORD=perf; shift "$shft_cnt";;
        --clean-tests)
            printf '%s\n' 'Deleting all test directories'
            rm -rf -- "$BLD"/test-*; shift "$shft_cnt";;
        --clean-seeds)
            printf '%s\n' 'Deleting all cached preseeds'
            rm -rf -- "$BLD"/preseeds; shift "$shft_cnt";;
        --clean)
            printf '%s\n' 'Deleting all tests and preseeds'
            rm -rf -- "$BLD"/test-* "$BLD"/preseeds; shift "$shft_cnt";;
        -d*|--debugger*)
            case "$optarg" in
                gdb) DBG="$GDB";;
                rr)  DBG="$RR";;
                *)   DBG="$optarg";;
            esac
            shift "$shft_cnt";;
        -c*|--config*)
            if [[ "$optarg" = /* ]]; then
                ORIG_CFG="${optarg}"
            else
                ORIG_CFG="$CWD/$optarg"
            fi
            shift "$shft_cnt";;
        -h|--help) _help=1; shift "$shft_cnt";;
        *)
            printf 'Unrecognized option: %s\n' "$1"
            _help=1; _err=1;
            break;;
    esac
done

case "$DURATION" in
    inf|infinity) DURATION=infinity;;
    '') DURATION=30;;
esac

if [[ -n "$_help" ]]; then
    printf "$usage" "$(basename $0)"
    exit "$_err"
fi

if [[ -z "$ORIG_CFG" ]]; then
    printf '%s\n' 'No config specified; exiting'
    exit 0
fi

# locate and move to test directory
mkdir -p "$TESTDIR"
printf 'Running test from %s\n' "$TESTDIR"
cd "$TESTDIR" || exit

# normalizes ports for local execution
IFS='' read -r -d '' normalize <<'EOF'
BEGIN {
    i = 29800
}

/".*:...."/ {
    gsub(/".*:...."/, "\"""0.0.0.0:" i "\"");
    ++i
}

{ print }
EOF

CFG="$TESTDIR"/config
awk "$normalize" "$ORIG_CFG" > "$CFG"

twophase=$(grep -q '2pc=1' "$CFG" && printf '1\n' || printf '0\n')
arch=
if test "$twophase" -eq 0; then
    arch='atomizer'
else
    arch='2pc'
fi

PERFS=
on_int() {
    printf 'Interrupting all components\n'
    trap '' SIGINT # avoid interrupting ourself
    for i in $PIDS; do # intentionally unquoted
        if [[ -n "RECORD" ]]; then
            kill -SIGINT -- "-$i"
        else
            kill -SIGINT -- "$i"
        fi
    done
    wait
    sleep 5

    _failed=
    for i in "$TESTDIR"/tx_samples_*.txt; do
        if ! test -s "$i"; then
            printf 'Could not generate plots: %s is not a non-empty, regular file\n' "$i"
            _failed=1
            break
        fi
    done

    if [[ "$RECORD" = 'perf' ]]; then
        for i in $PERFS; do
            kill -SIGTERM -- "$i"
        done
    fi

    if [[ -x "$(which flamegraph.pl)" && -x "$(which stackcollapse-perf.pl)" && -n "$(find "$TESTDIR" -maxdepth 1 -name '*.perf' -print -quit)" ]]; then
        printf 'Generating Flamegraphs\n'
        for i in "$TESTDIR"/*.perf; do
            waitpid -t 5 -e $(lsof -Qt "$i") &>/dev/null
            perf script -i "$i" | stackcollapse-perf.pl > "${i/.perf/.folded}"
            flamegraph.pl "${i/.perf/.folded}" > "${i/.perf/.svg}"
            rm -- "${i/.perf/.folded}"
        done
    fi

    if [[ -z "$_failed" ]]; then
        printf 'Generating plots\n'
        source "${RT}/scripts/activate-venv.sh"
        python "$RT"/scripts/plot-samples.py -d "$TESTDIR"
        deactivate
    fi

    printf 'Terminating any remaining processes\n'
    for i in $PIDS; do # intentionally unquoted
        if [[ -n "RECORD" ]]; then
            kill -SIGTERM -- "-$i"
        else
            kill -SIGTERM -- "$i"
        fi
    done
}

trap on_int SIGINT

getcount() {
    count=$(grep -E "$1_count" "$CFG")
    if test "$count"; then
        printf '%s\n' "$count" | cut -d'=' -f2
    else
        printf '0\n'
    fi
}

getpath() {
    case "$1" in
        # uniquely-named
        archiver)     printf '%s/src/uhs/atomizer/archiver/archiverd\n' "$BLD";;
        atomizer)     printf '%s/src/uhs/atomizer/atomizer/atomizer-raftd\n' "$BLD";;
        watchtower)   printf '%s/src/uhs/atomizer/watchtower/watchtowerd\n' "$BLD";;
        coordinator)  printf '%s/src/uhs/twophase/coordinator/coordinatord\n' "$BLD";;

        # special-case
        seeder)       printf '%s/tools/shard-seeder/shard-seeder\n' "$BLD";;

        # architecture-dependent
        loadgen)
            if test "$twophase" -eq 1; then
                printf '%s/tools/bench/twophase-gen\n' "$BLD"
            else
                printf '%s/tools/bench/atomizer-cli-watchtower\n' "$BLD"
            fi;;
        shard)
            if test "$twophase" -eq 1; then
                printf '%s/src/uhs/twophase/locking_shard/locking-shardd\n' "$BLD"
            else
                printf '%s/src/uhs/atomizer/shard/shardd\n' "$BLD"
            fi;;
        sentinel)
            if test "$twophase" -eq 1; then
                printf '%s/src/uhs/twophase/sentinel_2pc/sentineld-2pc\n' "$BLD"
            else
                printf '%s/src/uhs/atomizer/sentinel/sentineld\n' "$BLD"
            fi;;
        *) printf 'Unrecognized component: %s\n' "$1";;
    esac
}

run() {
    PROC_LOG="$TESTDIR"/"$PNAME.log"
    PERF_LOG="$TESTDIR"/"$PNAME-perf.log"
    COMP=
    case "$RECORD" in
        perf)
            $@ &> "$PROC_LOG" &
            COMP="$!"
            perf record -F 99 -a -g -o "$PNAME".perf -p "$COMP" &> "$PERF_LOG" &
            PERFS="$PERFS $!";;
        debug)
            ${DBG} "$@" &> "$PROC_LOG" &
            COMP="$!";;
        *)
            $@ &> "$PROC_LOG" &
            COMP="$!";;
    esac

    if test -n "$BLOCK"; then
        wait "$COMP"
    fi

    echo "$COMP"
}

seed() {
    seed_from=$(grep -E 'seed_from=.*' "$CFG" | cut -d'=' -f2)
    seed_from="${seed_from:-0}"
    seed_to=$(grep -E 'seed_to=.*' "$CFG" | cut -d'=' -f2)
    seed_to="${seed_to:-0}"
    seed_count=$(( "$seed_to" - "$seed_from" ))
    if test ! "$seed_to" -gt "$seed_from"; then
        printf 'Running without seeding\n'
        return
    fi

    preseed_id="$arch"_"$COMMIT"_"$seed_count"
    if test ! -e "$SEEDDIR"/"$preseed_id"; then
        printf 'Creating %s\n' "$preseed_id"
        mkdir -p -- "$SEEDDIR"/"$preseed_id"
        pushd "$SEEDDIR"/"$preseed_id" &> /dev/null
        PID=$(PNAME=seeder BLOCK=1 run "$(getpath seeder)" "$CFG")
        popd &> /dev/null
    fi

    printf 'Using %s as seed\n' "$preseed_id"
    for i in "$SEEDDIR"/"$preseed_id"/*; do
        ln -sf -- "$i" "$TESTDIR"/"$(basename "$i")"
    done
}

getpgid() {
    ps -o pgid= "$1"
}

PIDS=
launch() {
    last=$(getcount "$1")
    if test "$last" -le 0; then
        if test "$1" = 'loadgen'; then
            printf 'Running without a loadgen\n'
        else
            printf 'Invalid count for %s\n' "$1"
            exit 1
        fi
    else
        for id in $(seq 0 $(( "$last" - 1 )) ); do
            raft=$(getcount "$1$id")
            PNAME=
            if test "$raft" -gt 0; then
                for node in $(seq 0 $(( "$raft" - 1 )) ); do
                    export PNAME="$1${id}_$node"
                    PID=$(run "$(getpath "$1")" "$CFG" "$id" "$node")
                    for ep in $(awk -F'[":]' "/$PNAME.*endpoint/ { print \$3 }" "$CFG"); do
                        "$RT"/scripts/wait-for-it.sh -q -t 5 -h localhost -p "$ep"
                    done
                    printf 'Launched logical %s %d, replica %d [PID: %d]\n' "$1" "$id" "$node" "$PID"
                    if [[ -n "RECORD" ]]; then
                        PIDS="$PIDS $(getpgid $PID)"
                    else
                        PIDS="$PIDS $PID"
                    fi
                done
            else
                export PNAME="$1${id}"
                PID=$(run "$(getpath "$1")" "$CFG" "$id")
                for ep in $(awk -F'[":]' "/$PNAME.*endpoint/ { print \$3 }" "$CFG"); do
                    "$RT"/scripts/wait-for-it.sh -q -t 5 -h localhost -p "$ep"
                done
                printf 'Launched %s %d [PID: %d]\n' "$1" "$id" "$PID"
                if [[ -n "RECORD" ]]; then
                    PIDS="$PIDS $(getpgid $PID)"
                else
                    PIDS="$PIDS $PID"
                fi
            fi
        done
    fi
}

seed

if test "$twophase" -eq 0; then # atomizer
    for comp in watchtower atomizer archiver shard sentinel loadgen; do
        launch "$comp"
    done
else # twophase
    for comp in shard coordinator sentinel loadgen; do
        launch "$comp"
    done
fi

printf 'Awaiting manual termination or timeout (%ds)\n' "$DURATION"
sleep "$DURATION"

on_int
