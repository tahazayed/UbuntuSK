#!/bin/bash


./cbp2make.linux-x86_64 -in SKDownloader.cbp  -out Makefile -targets "Release"

git config credential.helper store

git add .

git commit -am "$1  [ci skip]"

git push

