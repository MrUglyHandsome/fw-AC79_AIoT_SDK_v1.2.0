#!/bin/sh

FILES="lib_a-strdup.o lib_a-atof.o"

function delete_files() {
    for FILE in $FILES; do
		if [ -f $FILE ] ; then
    		rm $FILE
			echo "Delete file : $FILE"
		fi
    done
}

mkdir Temp
cd Temp
ar -x ../libc.a 

delete_files

ar rcU libc.a *.o

cp -rf libc.a ../
cd ../
rm -rf Temp
