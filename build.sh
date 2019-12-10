#!/bin/bash
SOURCE_DIR=`pwd`
echo $SOURCE_DIR

if [ ! -e "build" ]
then
    mkdir "build"
fi

cd "build" \
    && cmake .. \
    && make

