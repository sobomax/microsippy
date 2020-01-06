#!/bin/sh

set -e

if [ -z "${IDF_TOOLCHAIN}" -o -z "${IDF_PATH}" ]
then
  echo "IDF_TOOLCHAIN and IDF_PATH need to be set" >&2
  exit 1
fi

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

IDF_TGT=${1:-"build"}

if [ "${IDF_TGT}" = "build" ]
then
  mkdir -p ${TOOLSDIR}/../src/build
  cmake -B${TOOLSDIR}/../src/build -H${TOOLSDIR}/../src
  make -C ${TOOLSDIR}/../src/build
else
  cd ${TOOLSDIR}/../src
  PATH="${PATH}:${IDF_TOOLCHAIN}/bin" python ${IDF_PATH}/tools/idf.py ${IDF_TGT}
fi
