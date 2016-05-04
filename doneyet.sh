#!/bin/bash

lockfile=nattorture.lock
done=false

while [ "$done" = "false" ]
do
if [ ! -f $lockfile ]; then
  done="true"
  sleep 1
fi
done
