#!/bin/sh

set -e

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

. "${TOOLSDIR}/test.common.sub"

IDF_TGT=${1:-"build"}

if [ "${IDF_TGT}" = "build" ]
then
  mkdir -p ${TOOLSDIR}/../src/build
  PATH="${PATH}:${IDF_TOOLCHAIN}/bin" cmake \
   -DWIFI_CONFIG=ON -B${TOOLSDIR}/../src/build -H${TOOLSDIR}/../src
  PATH="${PATH}:${IDF_TOOLCHAIN}/bin" make -C ${TOOLSDIR}/../src/build
  exit 0
fi

if [ "${IDF_TGT}" = "flash" ]
then
  ESPPORT=/dev/ttyUSB0 make -C ${TOOLSDIR}/../src/build flash
else
  cd ${TOOLSDIR}/../src
  PATH="${PATH}:${IDF_TOOLCHAIN}/bin" python ${IDF_PATH}/tools/idf.py ${IDF_TGT}
fi
