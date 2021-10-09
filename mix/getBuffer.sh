getToRPortBuffer(){
	printf_str=$1
	start_num=$2
	num=$3
	upper_port_start=$4	# 320;upper port's voq is normally printed
	incast_port=$5	# 32; #host * 2
	port_num=$6	# 20; #host_per_tor+#core

	echo $printf_str

	# get max buffers of upper port
	buffers=()
	buffers_upper=()
	buffers_down=()
	for ((i=0;i<num;i++));do
		index=`expr $i + $start_num`
		curr=`awk 'BEGIN{
			max = 0
			max_upper = 0
			max_down = 0
			curr_upper = 0
			curr_down = 0
			curr_incast_port = 0
			incast_port_when_max = 0
		}
		{
			if ($2 >= '$upper_port_start') {
				curr_upper += $5
			}else{
				curr_down += $5
			}
			if ($2%'$incast_port' == 1)
				curr_incast_port = $5
			if (NR%'$port_num' == 0){
				if (max_down < curr_down)
					max_down = curr_down
				if (max_upper < curr_upper)
					max_upper = curr_upper
				if (max < $8){
					max = $8
					incast_port_when_max = curr_incast_port
				}
				curr_down = 0
				curr_upper = 0
			}
		} END{printf max_upper; printf " "; printf max_down; printf " "; printf max; printf " "; printf incast_port_when_max;}' $path/${suffix}-buffer${index}.log`

		OLD_IFS="$IFS"
		IFS=" "
		curr_arr=($curr)
		IFS="$OLD_IFS"

		buffers_upper[$i]=${curr_arr[0]}
		buffers_down[$i]=${curr_arr[1]}
		buffers[$i]=${curr_arr[2]}
		buffers_incast_port[$i]=${curr_arr[3]}
	done
	echo "buffers: ${buffers[@]}"
	echo "buffers_upper: ${buffers_upper[@]}"
	echo "buffers_down: ${buffers_down[@]}"

	# calculate avg&max
	avg_whole=0
	max_whole=0
	num_whole=0
	avg_upper=0
	max_upper=0
	num_upper=0
	avg_down=0
	max_down=0
	num_down=0
	for ((i=0;i<num;i++));do
		if [[ $max_whole -le ${buffers[$i]} ]];then
			max_whole=${buffers[$i]}
		fi
		if [[ $max_upper -le ${buffers_upper[$i]} ]];then
			max_upper=${buffers_upper[$i]}
		fi
		if [[ $max_down -le ${buffers_down[$i]} ]];then
			max_down=${buffers_down[$i]}
		fi
		if [[ ${buffers_upper[$i]} -gt 0 ]]; then
			num_upper=`expr $num_upper + 1`
		fi
		if [[ ${buffers_down[$i]} -gt 0 ]]; then
			num_down=`expr $num_down + 1`
		fi
		if [[ ${buffers[$i]} -gt 0 ]]; then
			num_whole=`expr $num_whole + 1`
		fi
		avg_whole=`expr $avg_whole + ${buffers[$i]}`
		avg_upper=`expr $avg_upper + ${buffers_upper[$i]}`
		avg_down=`expr $avg_down + ${buffers_down[$i]}`
	done
	avg_whole=`echo "scale=1;$avg_whole/$num_whole" | bc`
	avg_upper=`echo "scale=1;$avg_upper/$num_upper" | bc`
	avg_down=`echo "scale=1;$avg_down/$num_down" | bc`
	echo "avg_whole, max_whole, avg_upper, max_upper, avg_down, max_down:"
	echo "$avg_whole"
	echo "$max_whole"
	echo "$avg_upper"
	echo "$max_upper"
	echo "$avg_down"
	echo "$max_down"
	echo "--------------------------------------"
}

getBuffer(){
	printf_str=$1
	start_num=$2
	num=$3

	echo $printf_str

	# get max buffers
	buffers=()
	for ((i=0;i<num;i++));do
		index=`expr $i + $start_num`
		buffers[$i]=`awk 'BEGIN{max = 0} {if ($8 > max) {max = $8}} END{printf max}' $path/${suffix}-buffer${index}.log`
	done
	echo "Buffers: ${buffers[@]}"

	# calculate avg&max
	avg=0;
	max=0;
	for ((i=0;i<num;i++));do
		if [[ $max -le ${buffers[$i]} ]];then
			max=${buffers[$i]}
		fi
		avg=`expr $avg + ${buffers[$i]}`
	done
	avg=`echo "scale=1;$avg/$num" | bc`
	echo "avg_max, max:"
	echo "$avg"
	echo "$max"
	echo "--------------------------------------"
}

if [[ $1 == "-h" ]]; then
	echo "usage: $0 [path] [suffix] [tor_num] [core_num]"
	echo "Attention: This script can only adapts to the 2-tier topology. However, only small changes are needed to adapt to the 3-tier topology."
else
	path=$1
	suffix=$2
	tor_num=$3
	agg_num=$4

	echo "$path/$suffix-bufferx.log"
	getToRPortBuffer "ToR statistic" 0 $tor_num 320 32 20

	getBuffer "Core statistics" $tor_num $agg_num

fi
