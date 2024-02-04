### SparkAGGR ShuffleManager Plugin
---

SparkAGGR is a high performance ShuffleManager plugin for Apache Spark that uses AGGR (instead of TCP) when performing Shuffle data transfers in Spark jobs.

### Runtime Requirements

---

* [Apache Spark 2.3.0](https://archive.apache.org/dist/spark/spark-2.3.0/spark-2.3.0-bin-hadoop2.7.tgz)
* [DPDK 18.11](http://fast.dpdk.org/rel/dpdk-18.11.tar.xz)
* [Mellanox OFED 4.5](https://www.mellanox.com/products/infiniband-drivers/linux/mlnx_ofed)
* [Java Native Access](https://mvnrepository.com/artifact/net.java.dev.jna/jna)

### Installation 

---

1. Obtain a clone of [SparkAGGR](https://github.com/yongchaoHe/kv-aggr)

2. Build the plugin, `make`

3. Configuration

   Firstly, go to `${SPARK_HOME}/conf` and provide Spark the location of the SparkAGGR plugin jars by using the extraClassPath option. 

   ```shell
   cp spark-defaults.conf.template spark-defaults.conf
   spark.driver.extraClassPath   ${SparkAGGR_DIR}/target/spark-aggr-1.0-for-spark-2.3.0-jar-with-dependencies.jar
   spark.executor.extraClassPath ${SparkAGGR_DIR}/target/spark-aggr-1.0-for-spark-2.3.0-jar-with-dependencies.jar
   ```

   Then, add the following line to `spark-defaults.conf` to enable the Spark ShuffleManager Plugin.

   ```shell
   spark.shuffle.manager         org.apache.spark.shuffle.rdma.AGGRShuffleManager
   ```

   

