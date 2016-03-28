#!/usr/bin/env bash

if test "$#" -ne 1; then
	echo "Illegal number of parameters: 1 expected"
	exit
fi
result=$(find -L $1 -type l -mtime +7)
for file in $result
do
	echo $file
done
