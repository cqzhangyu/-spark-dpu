LIB     := libspark_dpu.so
CC      := gcc
CFLAGS  := -O3
LDFLAGS := -fPIC -D_REENTRANT -I /usr/lib/jvm/java-8-openjdk-amd64/include/ -I /usr/lib/jvm/java-8-openjdk-amd64/include/linux/ -shared -lrt -lpthread 
# javah -jni -classpath ~/code/spark-aggr/core/src/main/java/ org.apache.spark.DPDK
# javah -jni -classpath ~/code/kv-aggr/SparkAGGR/target/classes org.apache.spark.shuffle.aggr.DPDK

all:main

main: org_apache_spark_shuffle_aggr_DPDK.c
	$(CC) $(CFLAGS) -o $(LIB) org_apache_spark_shuffle_aggr_DPDK.c $(LDFLAGS)

.PHONY: clean
clean:
	rm $(LIB)

