#!/bin/bash
#deploy files to uip atari tool via http

#remove trailing backslash
local_path=${1%/}
host_path=${2%/}

shopt -s extglob

function curl_commandline {
    while read line
    do
        unique=${line%/*}
        part_path=${unique##*${local_path}}
        remote_path=$(printf " -T $line --url $host_path/%s%s " ${part_path#/} "/")
        #remove any double slashes - TOS doesn't like it!
        remote_path=${remote_path//\/*(\/)/\/}
        echo $remote_path
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
local_files=$(hash_files)
files_to_send=""
sum_path=${host_path}/md5sum.gz
#fetch remote timestamp file
sums=$(curl -s -f --url ${sum_path} | gzip -d -)
if (($? > 0)); then
    files_to_send=$local_files 
else
	files_to_send=$(printf "$sums" | md5sum --quiet -c - 2> /dev/null | grep FAILED | sed -e 's/\(.*:\).*/\1/' -e 's/://g' )
fi

echo "$files_to_send" | curl_commandline

if [ -n "$files_to_send" ]; then
	files=$(md5sum $local_files)
    curl -s -H "Expect:" $(echo "$files_to_send" | curl_commandline) 2>&1
	printf "$files" | gzip -q -c - | curl -0 -X PUT --data-binary @- ${sum_path}
else
	echo "all up to date"
fi
