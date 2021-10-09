if [[ $1 == "-h" ]];then
	echo "usage: $0 [output file] [path of realtime buffer/bw file dir]"
	echo -e "Attention: \n1. Make sure the required scripts existed at the same path with this script file: cutData.sh, calcuIncastmix.sh, getBuffer.sh, getQueuingTime.sh."
	echo -e "2. This script only fits leafspine-10ToR-4Core.topo. For other topologies, you need to modify the parameters of \"getBuffer.sh\"."
else
	log_file=$1
	realtime_path=$2
	 
	# realtime buffer/bw file prefix
	realtime_prefix=`grep "REALTIME_BUFFER_BW" $log_file | awk '{print $2}'`
	realtime_prefix=`basename $realtime_prefix`
	
	# statistics in output file
	tail -55 $log_file
	echo "------------------------------------"

	# cut data
	./cutData.sh $log_file Ordered
	filename=${log_file%.*}   # $filename".data"

	# the number of incast flows
	incast_times=`grep "incast_time" $log_file | awk '{print $2}'`
	incast_flows_one_time=`grep "INCAST_MIX" $log_file | awk '{print $2}'`
	incast_num=`expr $incast_times \* $incast_flows_one_time`
	all_num=`wc -l $filename.data | awk '{print $1}'`
	poisson_num=`expr $all_num - $incast_num`

	# poisson and incast FCT
	./calcuIncastmix.sh $filename.data 10 $incast_num
	echo "------------------------------------"

	# buffer info from realtime buffer output
	./getBuffer.sh $realtime_path $realtime_prefix 10 4

	# queuing time of poisson flows at each hop
	./getQueuingDelay.sh $realtime_path/$realtime_prefix-dataQueuing.log $poisson_num

fi
