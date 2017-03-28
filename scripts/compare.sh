#!/usr/bin/env bash

scriptdir="$(dirname "$0")"

configurations() {
    sqlite3 "$scriptdir"/../data/db.sqlite 'SELECT DISTINCT(configuration) FROM formulas'
}

while true
do
cs=()
i=0
while read c
do
    echo "$i: $c"
    cs+=("$c")
    i=$((i+1))
done < <(configurations)

echo -n "enter index A: "; read ai
echo -n "enter index B: "; read bi

A=${cs[$ai]}
B=${cs[$bi]}

sqlite3 "$scriptdir"/../data/db.sqlite <<EOF
SELECT printf("%50s: clauses %6d, longest clause %5d, A %6d, B %6d, removed len A %3f, B %3f, times A %6f, B %6f, difference %6f", a.name, a.clauses, a.max_clause_length, a.clauses_preprocessed, b.clauses_preprocessed, a.removed_clauses_average_length, b.removed_clauses_average_length, a.time, b.time, a.time - b.time)
FROM formulas a, formulas b
WHERE a.configuration = "$A" AND b.configuration = "$B"
AND a.name = b.name
ORDER BY a.time
;
EOF
read
done
