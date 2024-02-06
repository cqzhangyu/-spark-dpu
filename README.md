# SparkDPU

## System Requirements

* [Apache Spark 2.3.0](https://archive.apache.org/dist/spark/spark-2.3.0/spark-2.3.0-bin-hadoop2.7.tgz)
* Java 8
* scala 2.11.12
* Maven
* sbt

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
> 注意：请在DPU的host端运行这个脚本。
* 输出文件解释：
    * `driver_log`: Spark的driver的输出。
    * `ipc_output`: host的Spark进程对接DPU的client的部分（JNI）的输出。
    * `dpu_*.txt`: 第`*`个DPU上的进程的输出。
