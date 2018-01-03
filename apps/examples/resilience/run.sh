#!/bin/bash

TARGET=resilience
PLAT=x86-mpi
NUM_THREADS=4
NUM_NODES=2
APP_ROOT=${PWD}

if [[ -z "${OCR_INSTALL}" ]]; then
    echo "ERROR: Please set environment variable OCR_INSTALL"
    exit 1
fi

LD_LIBRARY_PATH=${OCR_INSTALL}/lib:${LD_LIBRARY_PATH}

APP_BUILD=${APP_ROOT}/build/$PLAT
APP_INSTALL=${APP_ROOT}/install/$PLAT

if [[ ! -d "${APP_INSTALL}" ]]; then
    make -f Makefile.${PLAT}
fi

if [[ ! -d "${APP_BUILD}" ]]; then
    echo "ERROR: Cannot find build directory"
    exit 2
fi

cp ${APP_BUILD}/${TARGET} ${APP_INSTALL}/${TARGET}

${OCR_INSTALL}/share/ocr/scripts/Configs/config-generator.py --remove-destination --threads ${NUM_THREADS} --output ${APP_INSTALL}/generated.cfg --target mpi --guid COUNTED_MAP

cd ${APP_INSTALL}

mpirun -np ${NUM_NODES} ${APP_INSTALL}/${TARGET} -ocr:cfg ${APP_INSTALL}/generated.cfg

cd ${APP_ROOT}
