#!/usr/bin/env bash

scriptdir="$(dirname "$0")"
# cd "$scriptdir/.."

old="$1"
new="$1.new"

rm -f "$new"

sqlite3 "$new" < "$scriptdir/schema.sql"

for table in formula preprocessing
do

cols="$(sqlite3 db.sqlite ".dump $table" | grep -v '\<solving_time_lingeling\>' | grep '^    ' | sed \$d | awk '{printf "%s, ", $1 }' | sed 's/, $//')"
sqlite3 "$old" <<EOF | sed "s/VALUES/($cols) VALUES/" | sqlite3 "$new"
.mode insert $table
select $cols from $table;
EOF

done

sqlite3 "$old" 'select printf('"'"'INSERT INTO solving_time (name, commit_id, configuration, preprocessed_solving_time_lingeling) VALUES ("%s", "%s", "%s", %f);'"'"', name, commit_id, configuration, solving_time_lingeling) from preprocessing' | sqlite3 "$new"
