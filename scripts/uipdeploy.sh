#!/bin/bash
#deploy files to uip atari tool via http

local_path=$1
host_path=$2

function curl_upload_arg {
    line=$1
    unique=${line/*}
    printf " -T $line --url $host_path/%s%s" ${unique##*${local_path}} "/ "    
}

function find_files {
    find $local_path -type f | \
        while read line
        do
            unique=${line%/*}
            printf " -T $line --url $host_path/%s%s" ${unique##*${local_path}} "/ "  
        done
}

curl -v -H "Expect:" $(find_files) 2>&1 | grep PUT

#echo $(curl_upload_arg /cygdrive/e/Trash/sndh_sf/history.txt)
#echo $(curl_upload_arg /cygdrive/e/Trash/sndh_sf/bla/bla/history.txt)