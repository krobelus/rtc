#!/usr/bin/env bash

set -e

scriptdir="$(dirname "$0")"
cd "$scriptdir/.."

source ../scripts/common.sh

db=db.sqlite
bm=../benchmarks/sc14/app/preprocessed
preprocessed=/tmp/preprocessed
proof=/tmp/proof

sql() {
    echo "$@ ;"
    echo "$@ ;" | sqlite3 "$db"
}

have() {
    test -n "$(echo "$@ ;" | sqlite3 "$db")"
}

test -f "$db" || sqlite3 "$db" < scripts/schema.sql

formulas=$bm/*.cnf
# formulas="$(ls $bm/*.cnf | sed 10q)"
# formulas=`echo ../benchmarks/crafted/{001,002,003}.orig`

fill_formulas() {
    for f in $formulas
    do
        name="$(basename "$f")"
        have "select * from formula where name = \"$name\"" && continue
        variables="$(variables "$f")"
        clauses="$(clauses "$f")"
        max_clause_length="$(max_clause_length "$f")"
        original_solving_time_lingeling=NULL
        sql \
            "insert into formula (name, variables, clauses, max_clause_length, original_solving_time_lingeling)"\
            "values (\"$name\",$variables,$clauses,$max_clause_length,$original_solving_time_lingeling)"\
            || { echo ^ dw; break; }

    done
}

compute_original_solving_times() {
    for f in $formulas
    do
        name="$(basename "$f")"
        have "select * from formula where name = \"$name\" and original_solving_time_lingeling is not null" && continue
        echo running lingeling on original cnf "$f"
        set +e
        original_solving_time_lingeling="$(lgl "$f")"
        set -e
        sql \
            "update formula set original_solving_time_lingeling = $original_solving_time_lingeling "\
            "where name = \"$name\""
    done
}

fill_L_sizes() {
    echo 'LOG_L_SIZES:=true' > config.mk
    make rtc

    for f in $formulas
    do
        name="$(basename "$f")"
        have "select * from L_sizes where name = \"$name\""\
            && continue
        source <(./rtc "$f" "$preprocessed" "$proof")
        sql \
    "insert into L_sizes"\
    "(name, s1, s2, s3, s4)"\
    "values"\
    "(\"$name\",$l_size_1,$l_size_2,$l_size_3,$l_size_4)"\
    || true
    done
    rm -f config.mk
}

run_config() {
    configuration="$1"
    solving_time="$2"
    for feature in $configuration; do
        echo "$feature"
    done > config.mk
    make rtc

    for f in $formulas
    do
        name="$(basename "$f")"
        if [ -n "$solving_time" ]; then
            have "select * from preprocessing p, solving_time t where p.name = t.name and p.configuration = t.configuration and p.name = \"$name\" and" \
                "p.configuration = \"$configuration\"" \
                && continue
        else
            have "select * from preprocessing where name = \"$name\" and configuration = \"$configuration\"" \
                && continue
        fi
        time1="$(date +%s.%N)"
        source <(./rtc "$f" "$preprocessed" "$proof")
        time2="$(date +%s.%N)"
        ../checkers/check-orig-drat-and-reverse-rat.sh "$f" "$proof" "$preprocessed" >/dev/null \
            || log "*** rat proof failed: $name"
        dspr-trim "$preprocessed" /tmp/reverse.proof -f -v | grep -q "all lemmas verified" \
            || die "*** spr proof failed: $name"
        time="$(echo "$time2 - $time1" | bc)"
        removed_clauses_average_length="$(average_length "$proof")"
        clauses_preprocessed="$(clauses "$preprocessed")"
        if [ -n "$solving_time" ]; then
            echo running lingeling on preprocessed formula of "$f"
            preprocessed_solving_time_lingeling="$(lgl "$preprocessed")"
            sql \
                "insert into solving_time"\
                "(name, commit_id, configuration, preprocessed_solving_time_lingeling)"\
                "values"\
                "(\"$name\",\"$commit_id\",\"$configuration\",$preprocessed_solving_time_lingeling)"
        fi
        have "select * from preprocessing where name = \"$name\" and configuration = \"$configuration\"" \
            && continue
        sql \
    "insert into preprocessing"\
    "(name, commit_id, configuration, time, clauses_preprocessed, removed_clauses_average_length)"\
    "values"\
    "(\"$name\",\"$commit_id\",\"$configuration\",$time,$clauses_preprocessed,$removed_clauses_average_length)"\
    || true
    done
    rm -f config.mk
}

configs=(
''
'PROPERTY:=RAT'
'WATCH_LITERALS:=true'
'SORT_VARIABLE_OCCURRENCES:=false'
'STORE_REFUTATIONS:=false'
'DETECT_ENVIRONMENT_CHANGE:=false'
'SMALL_CLAUSE_OPTIMIZATION:=false'
'PARALLEL:=PTHREADS THREADS:=3'
'PARALLEL:=PTHREADS THREADS:=4'
'PARALLEL:=LACE'
)

configs_solving_time=(
''
)

fill_formulas
fill_L_sizes
compute_original_solving_times

for config in "${configs[@]}"; do
    echo "config: $config"
    run_config "$config"
done

for config in "${configs_solving_time[@]}"; do
    echo "config: $config"
    run_config "$config" solving_time
done
