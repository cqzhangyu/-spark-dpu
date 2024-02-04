# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

LIB     := libspark_dpu.so
CC      := gcc
CPP		:= g++
CFLAGS  := -O3
CPPFLAGS := -O3
LDFLAGS := -fPIC -D_REENTRANT -I /usr/lib/jvm/java-8-openjdk-amd64/include/ -I /usr/lib/jvm/java-8-openjdk-amd64/include/linux/ -shared -lrt -lstdc++

CSRCS := $(wildcard *.c *.c)
COBJS := $(CSRCS:.c=.o)

CPPSRCS := $(wildcard *.cpp *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)

LDFLAGS += -pthread -lrt
CFLAGS += -O3 $(shell pkg-config --cflags doca)
LDFLAGS = $(shell pkg-config --libs doca)

# javah -jni -classpath ~/code/spark-dpu/core/src/main/java/ org.apache.spark.DPDK
# javah -jni -classpath ~/code/spark-dpu/SparkDPU/target/classes org.apache.spark.shuffle.aggr.DPDK

SRCS := org_apache_spark_shuffle_aggr_DPDK.c dma_copy_core.c common.c utils.c pack.c

all:main

$(COBJS) : %.o: %.c
	$(CC) -c $< 

main: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(LIB) $(LDFLAGS)

.PHONY: clean
clean:
	rm $(LIB)
