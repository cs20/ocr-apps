#!/bin/bash

root="JOB_XSBench"

rm ${root}.post
rm ${root}1.post

for nodes in 1; do

    pfile=${root}_$nodes.out

    grep "noProf " $pfile >> ${root}1.post

    numRuns=`grep 'noProf ' $pfile | wc -l`

    for ((k=1;k<=numRuns;k++)); do
        tfile="temp.file"
        awk -v n="$k" '/noProf / {f=0}; f && c==n; /noProf / {f=1; c++}' $pfile > temp.file

        matchFound=`grep 'Lookups/s' $tfile | wc -l`
        if [[ $matchFound != 0 ]]; then
            a=`grep 'Lookups/s' $tfile | awk '{print $2}' | tr -d ',' | awk -v nexp="${matchFound}" '{sum1+=$1;} (NR%nexp)==0{print sum1/nexp; sum1=0;}'`
            b=`grep 'Runtime' $tfile | awk '{print $2}' | awk -v nexp="${matchFound}" '{sum1+=$1;} (NR%nexp)==0{print sum1/nexp; sum1=0;}'`
        else
            a="-"
            b="-"
        fi

        echo $a $b
        echo $a $b >> ${root}.post
    done

done