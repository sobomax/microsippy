#!/bin/sh

set -e
set -x

if [ -z "${IDF_TOOLCHAIN}" -o -z "${IDF_PATH}" ]
then
  echo "IDF_TOOLCHAIN and IDF_PATH need to be set" >&2
  exit 1
fi

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

cd ${TOOLSDIR}/../src
PATH="${PATH}:${IDF_TOOLCHAIN}/bin" python "${TOOLSDIR}/ptyrun.py" ${IDF_PATH}/tools/idf.py monitor | tee monitor.log&
MON_RC=${?}
MON_PID=${!}
sleep 20
kill -TERM ${MON_PID}
wait ${MON_PID} || true
grep 'example: Waiting for data' monitor.log
