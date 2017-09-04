#!/bin/bash

for i in {1..500}
do
   docker stop systemd$i
   docker rm systemd$i
done
docker rm $(docker ps -qa --no-trunc --filter "status=exited")


