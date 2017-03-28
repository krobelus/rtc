#!/usr/bin/env fish


set scriptdir (dirname (status -f))

cd $scriptdir/..

for f in ../benchmarks/sc14/app/preprocessed/*.cnf
# for f in ../benchmarks/sc14/app/preprocessed/{010-22-144,010-23-80,006-22-144}.cnf
    echo $f
    ./rtc $f preprocessed proof
    ../scripts/java-preprocessor.sh preprocessed preprocessed.jpp proof.jpp
    set a (cat proof.jpp)
    test -z "$a"; or begin
        echo different fixpoint!!!: $f
        exit 1
    end
end

echo success
