### Key-Value Aggregation
---

kv-aggr is a streaming protocol for in-network key value aggregation.

---

## System Requirements

* [Apache Spark 2.3.0](https://archive.apache.org/dist/spark/spark-2.3.0/spark-2.3.0-bin-hadoop2.7.tgz)
* Java 8
* Maven
* sbt

## Configurations

* By default, this repo is installed at `/home/${username}/code/` on each machine. Spark is installed at `/usr/local/spark`. Immediate files are stored in `/home/${username}/spark`. 
* Make sure that `${username}` has ssh \& sudo no password authority on each machine.
* Modify `SparkAGGR/src/main/java/org/apache/spark/shuffle/aggr/DPDK.java:39` to the path of immediate file `libkv_aggr.so`.
* 
* Topology: 3 hosts: worker1, worker2, worker3. Hosts are connected through a control network and a data network. Their IPs in the control network are `10.0.0.1`, `10.0.0.2`, `10.0.0.3`; IPs in the data network are `192.168.0.1`, `192.168.0.2`, `192.168.0.3`.
* To change the IPs of each workers, modify `scripts/my-test`, `scripts/my-compile`, `scripts/utils`. Find all `10.0.0.` and `dsts, worker_ids` and modify them.
* Besides, modify `SparkAGGR/dpdk/dpdk-common.c:ether_arr`, `SparkAGGR/dpdk/spark-opts.c:MACs`, `SparkAGGR/dpdk/spark-opts.c:IPs`.

## Compilation

One command:
```shell
bash scripts/my-compile.sh
```

> Notes
* The `.jar` binaries can be compiled on any machines (e.g. on your local PC, if you PC can successfully pull maven/sbt repositories using proxies/mirrors) and sent to the servers. The `dpu` modules (`SparkDPU`) are compiled on each server independently. Therefore, you may need some scripts to sync codes among servers.


## Run

One command:
```shell
bash scripts/my-test.sh [sort|aggr|sql] [reduce|group|sql]

e.g.
# Original Spark for reduce
bash scripts/my-test.sh sort reduce
# Original Spark for SQL
bash scripts/my-test.sh sort sql
# SparkAGGR for reduce
bash scripts/my-test.sh aggr reduce
# SparkAGGR(SQL) for SQL
bash scripts/my-test.sh sql sql
```
> Note
* This command runs on the master server (`10.0.0.1` in the previous configuration).
