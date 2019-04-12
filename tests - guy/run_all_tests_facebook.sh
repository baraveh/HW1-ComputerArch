#!/bin/bash
let "i = 1"
for filename in tests/test*.in; do
    test_num=`echo $filename | cut -d'.' -f1`
    #dos2unix ${filename}
    ./bp_main $filename > ${test_num}Yours.out
	echo "test number $i"
    diff ${test_num}.out ${test_num}Yours.out
	let "i++"
done