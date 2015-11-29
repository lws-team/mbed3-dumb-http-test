#!/bin/sh

if [ -z "$1" ] ; then
	echo "Usage: $0 <file>"
	echo "       Creates <file>.h ready to be included"
	exit 0
fi

hexdump -C -v $1 | \
	sed "s/^[^\ ]*\ //g" | \
	sed s/^[^\ ]*//g | \
	sed "s/|.*|//g" | \
	tr -s ' ' | \
	sed  "s/\ /,\ 0x/g" | \
	sed "s/^,\ //g" | \
	sed "s/,\ 0x$/,/g" > $1.h

