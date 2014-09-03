#!/bin/bash
#deploy files to uip atari tool via http

local_path=$1
host_path=$2

function curl_commandline {
    while read line
    do
        unique=${line%/*}
        printf " -T $line --url $host_path/%s%s" ${unique##*${local_path}} "/ "  
    done
}

function hash_files {
    find $local_path -type f | \
        while read line
        do
        	echo $line
        done
}
#echo $(hash_files)

files_to_send=""
sum_path=${host_path}/sum.md5
sums=$(curl -s -f --url ${sum_path} | gzip -d -)
if (($? > 0)); then
    files_to_send=$(hash_files) 
    md5sum $(hash_files) | gzip -c - | curl -0 -X PUT --data-binary @- ${sum_path}
else
	files_to_send=$(printf "$sums" | md5sum --quiet -c - 2> /dev/null | grep FAILED | sed -e 's/\(.*:\).*/\1/' -e 's/://g' )
fi


echo "$files_to_send" | curl_commandline


#curl -v -H "Expect:" $(find_files) 2>&1 | grep PUT

#echo $(curl_upload_arg /cygdrive/e/Trash/sndh_sf/history.txt)
#echo $(curl_upload_arg /cygdrive/e/Trash/sndh_sf/bla/bla/history.txt)