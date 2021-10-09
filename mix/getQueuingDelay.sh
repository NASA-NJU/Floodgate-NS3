if [[ $1 == "-h" ]]; then
	echo "usage: $0 [filename] [poisson_num]"
else
	filename=$1
	poisson_num=$2
	echo "$filename"
	awk 'BEGIN {
		sum_poisson_src = 0
		sum_poisson_core = 0
		sum_poisson_dst = 0
		num_poisson = 0
		sum_incast_src = 0
		sum_incast_core = 0
		sum_incast_dst = 0
		num_incast = 0
	} {
		if ($1 < '$poisson_num'){
			sum_poisson_src += $8
			sum_poisson_core += $9
			sum_poisson_dst += $10
			num_poisson += 1
		}else{
			sum_incast_src += $8
			sum_incast_core += $9
			sum_incast_dst += $10
			num_incast += 1
		}
	} END{
		print "poisson: srcToR core dstToR all"
		print sum_poisson_src/num_poisson/1e3
		print sum_poisson_core/num_poisson/1e3
		print sum_poisson_dst/num_poisson/1e3
		print (sum_poisson_src+sum_poisson_core+sum_poisson_dst)/num_poisson/1e3
		print "incast: srcToR core dstToR all"
		print sum_incast_src/num_incast/1e3
		print sum_incast_core/num_incast/1e3
		print sum_incast_dst/num_incast/1e3
		print (sum_incast_src+sum_incast_core+sum_incast_dst)/num_incast/1e3
	}' $filename
	echo "-----------------------------------------"
fi
