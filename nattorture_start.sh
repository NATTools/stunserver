#!/bin/bash

#A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

interface="eth0"
output_file="out.csv"
test_name="test"

lockfile=nattorture.lock
touch $lockfile

while getopts "h?i:f:r:o:t:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    r)  runs=$OPTARG
        ;;
    i)  interface=$OPTARG
        ;;
    f)  output_file=$OPTARG
        ;;
    t)  test_name=$OPTARG
        ;;
    esac
done

shift $((OPTIND-1))

[ "$1" = "--" ] && shift

printf "Time, transInUse, maxTransInuse, pkt cnt, max pkt_cnt, byte cnt, max byte cnt, kbps, max kbps, user, nice, system, idle, iowait\n" > $output_file
build/dist/bin/stunserver -i $interface --csv >> $output_file

rm $lockfile
