#!/bin/bash

. ./stopall.sh

docker run --rm --privileged -v /:/host tahazayed/ubuntusk:latest setup

for i in {1..50}
do
   docker run -d --name systemd$i --security-opt seccomp=unconfined --tmpfs /run --tmpfs /run/lock -v /sys/fs/cgroup:/sys/fs/cgroup:ro   tahazayed/ubuntusk:latest 
done
