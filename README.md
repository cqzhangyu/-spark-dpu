# SparkDPU

## System Requirements

* [Apache Spark 2.3.0](https://archive.apache.org/dist/spark/spark-2.3.0/spark-2.3.0-bin-hadoop2.7.tgz)
* Java 8
* scala 2.11.12
* Maven
* sbt
* cmake

## Configurations

* 两台host。分别是`192.168.200.1`和`192.168.200.2`。
* 两个host上各有一个DPU。从各自的host访问DPU的用户名和地址均为`ubuntu@192.168.100.2`。
* 如果要修改运行时用到的DPU编号，请修改`scripts/utils.sh`脚本。
* 为所有host和DPU之间配置ssh免密登录。这里我已经为用户名`zcq`配置好了免密登录。
* host的代码放在`/home/${user_name}/code/spark-dpu`中，生成的可执行文件以及运行结果都放在`/home/${user_name}/target`中。DPU的代码放在`/home/ubuntu/code/spark-dpu`中，生成的可执行文件以及运行结果都放在`/home/ubuntu/target`中。
    * 请自行同步这些目录中的所有代码（例如，用自己写的rsync脚本）
    * 编译脚本和运行脚本会自动从每台机器上的代码目录中取代码进行编译，同时将可执行文件以及运行结果放在`target`目录中。如需修改路径，请修改`scripts/utils.sh`脚本。
* host的spark安装在`/usr/local/spark`中。如需修改host的spark路径或java路径，或者dpu的java路径，请修改`scripts/utils.sh`脚本。


## Compilation

One command:
```shell
bash scripts/compile.sh
```

> 注意
* 编译需要从外网下载一些依赖的包，所以请在一台有良好代理的机器上运行这个compile脚本。脚本会自动将编译好的jar文件上传到各个机器上。
* 对于compile机器，DPU的host的远程主机名称是`pkudpuhost1`和`pkudpuhost2`，且用户名相同。如需修改，请修改`scripts/utils.sh`



## Run

One command:
```shell
bash scripts/run.sh [dpu|base] [reduce|group]

e.g.
# Original Spark for reduce
bash scripts/my-test.sh base reduce
# SparkDPU for reduce
bash scripts/my-test.sh dpu reduce
```
> 注意：
*   请在DPU的host端运行这个脚本。
*   运行的参数（数据大小，使用的机器编号，机器线程数，内存等）放在`scripts/utils.sh`中，请自行修改。


## Other

* 一些输出文件解释：
    * 默认在host的`/home/${user_name}/code/spark-dpu`目录下：
        * `driver_log`: Spark的driver的输出。
        * `ipc_output`: host的Spark进程对接DPU的client的部分（JNI）的输出。这个我暂时关闭了。
        * `dpu_*.txt`: 第`*`个DPU上的进程的输出。
    * 在host的`$SPARK_HOME`（默认`/usr/local/spark`）的`work/`中存放了每次运行应用时的worker的输出。

* 为了方便调试，你也许可以修改如下的代码
    * `SparkDPU/dma/src/org_apache_spark_shuffle_aggr_DPDK.cpp`中的`LOG_PATH`是一个硬编码路径，用于存放spark对接DPU模块(JNI)的输出。如果想要在调试中查看JNI模块的输出，可以修改`LOG_PATH`为你想要的路径，并启用`log_init`函数中的`log_open`。而且你可以调整`set_log_level`来实现你想要的log级别。
    * `SparkDPU/src/main/scala/org/apache/spark/shuffle/aggr/shared/AGGRSharedShuffleFetcher`的`splitLocalRemoteBlocks`函数会把将要读的块分为本地块和远端块进行处理，如果你只想处理本地/远端块，可以修改这个函数。
