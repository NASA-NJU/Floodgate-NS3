echoPercent(){
	file=$1
	lines=$2
	p=$3

	p_line=`echo "scale = 0; $lines * $p / 100" | bc`
	p_data=`cut -f $calcu_line $file | sort -n | head -$p_line | tail -1`
	echo "$p_data"
}

printPercentile(){
	filename=$1
	calcu_line=$2
	echo_str=$3

	line_num=`wc -l $filename | awk '{print $1}'`
	avg=`awk '{s += $'$calcu_line'} END {print s/NR}' "$filename"`
	echo "$echo_str: avg 25th 50th 90th 95th 99th"
	echo $avg
	echoPercent $filename $line_num 25
	echoPercent $filename $line_num 50
	echoPercent $filename $line_num 90
	echoPercent $filename $line_num 95
	echoPercent $filename $line_num 99
}

if [[ $1 == "-h" ]]; then
	echo "usage: $0 [path] [calcu_line] [tail_num]"
else
	file=$1
	calcu_line=$2
	incast_num=$3

	echo $file
	filename=${file%.*}
	filename_poisson="$filename-poisson.data"
	filename_incast="$filename-incast.data"
	all_num=`wc -l $file | awk '{print $1}'`
	poisson_num=`expr $all_num - $incast_num`

	echo "output $poisson_num: $filename_poisson"
	head -$poisson_num $file > $filename_poisson

	echo "output $incast_num: $filename_incast"
	tail -$incast_num $file > $filename_incast

	# poisson
	printPercentile $filename_poisson $calcu_line "poisson"
	# incast
	printPercentile $filename_incast $calcu_line "incast"

	rm -rf $filename_poisson
	rm -rf $filename_incast

fi
