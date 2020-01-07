#!/bin/sh

set -e
set -x

TOOLSPATH=`realpath ${0}`
TOOLSDIR=`dirname ${TOOLSPATH}`

. "${TOOLSDIR}/test.common.sub"

cd "${SRCDIR}"
PATH="${PATH}:${IDF_TOOLCHAIN}/bin" python "${TOOLSDIR}/ptyrun.py" -o monitor.log \
  ${IDF_PATH}/tools/idf.py monitor &
MON_RC=${?}
MON_PID=${!}
sleep 5
BRD_IP=`grep 'sta ip: ' monitor.log | sed 's|.*sta ip: ||; s|,.*||'`
if [ ! -z "${BRD_IP}" ]
then
  sed "s|%%BRD_IP%%|${BRD_IP}|g" ${TOOLSDIR}/100trying.raw | nc -w 1 -u "${BRD_IP}" 5060
  sleep 15
fi
kill -TERM ${MON_PID}
wait ${MON_PID} || true
if [ -z "${BRD_IP}" ]
then
  exit 1
fi
grep -q 'Waiting for data' monitor.log
grep -q 'SIP/2.0 100 Trying' monitor.log
