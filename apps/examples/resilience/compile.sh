#!/bin/bash

SRCS=resilience.c
TARGET=resilience

PLAT=x86-mpi
APP_ROOT=${PWD}
APP_BUILD=${APP_ROOT}/build/$PLAT
APP_INSTALL=${APP_ROOT}/install/$PLAT

if [[ -z "${OCR_INSTALL}" ]]; then
    echo "ERROR: Please set environment variable OCR_INSTALL"
    exit 1
fi

LD_LIBRARY_PATH=${OCR_INSTALL}/lib:${LD_LIBRARY_PATH}

dir=build
if [[ ! -d "${dir}" ]]; then
    mkdir ${dir}
fi

dir=build/$PLAT
if [[ ! -d "${dir}" ]]; then
    mkdir ${dir}
fi

dir=install
if [[ ! -d "${dir}" ]]; then
    mkdir ${dir}
fi

dir=install/$PLAT
if [[ ! -d "${dir}" ]]; then
    mkdir ${dir}
fi

mpicc -o ${APP_BUILD}/${TARGET} -Werror -I. -I${OCR_INSTALL}/include -DOCR_TYPE_H=x86-mpi.h -O2 ${SRCS} -L${OCR_INSTALL}/lib -locr_x86-mpi -lpthread
