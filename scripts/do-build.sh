#!/bin/sh

set -e

cd src
python ${IDF_PATH}/tools/idf.py build
