#!/bin/sh

set -e

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

cd ${TOOLSDIR}/../src
python ${IDF_PATH}/tools/idf.py build
