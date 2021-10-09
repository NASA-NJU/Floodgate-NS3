if [[ $1 == "-h" ]]; then
        echo "usage: $0 [file] [grep pattern]"
else
	file=$1
	grepPattern=$2
	
        filename=${file%.*}
	cutline=`grep "^qp finished" $file | tail -1 | awk '{print $3}'`
        echo "to cut $cutline data: $file > $filename.data"
        grep -A$cutline "$grepPattern" $file | tail -$cutline > $filename.data
fi
