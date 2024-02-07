#  host环境配置
## java

```
sudo apt install openjdk-8-jdk-headless
```

* 如果有多个版本的java

```
sudo update-alternatives --config java
```

* 测试

```
java -version
```



## scala

* 去官网找`scala-2.11.12.tgz`

[Install | The Scala Programming Language (scala-lang.org)](https://www.scala-lang.org/download/)

* scp到服务器上

```
tar xvf scala-2.11.12.tgz
sudo mv scala-2.11.12 /usr/local/scala-2.11.12
sudo ln -s /usr/local/scala-2.11.12 /usr/local/scala
```

* 在`.bashrc`中添加

```
export PATH=$PATH:/usr/local/scala/bin
```

* 测试

```
source .bashrc
scala -version
```



* 如果有多个版本

```
sudo rm /usr/bin/scala
sudo rm /usr/bin/scalac
sudo ln -s /usr/local/scala/bin/scala /usr/bin/scala
sudo ln -s /usr/local/scala/bin/scalac /usr/bin/scalac
```





## Spark

* 去官网找`spark-2.3.0-bin-hadoop2.7.tgz`

[Downloads | Apache Spark](https://spark.apache.org/downloads.html)

* scp上传到服务器上
* 

```
tar xvf spark-2.3.0-bin-hadoop2.7.tgz
sudo mv spark-2.3.0-bin-hadoop2.7 /usr/local/spark-2.3.0
sudo chmod -R +777 /usr/local/spark-2.3.0
```

```
sudo ln -s /usr/local/spark-2.3.0 /usr/local/spark
```

* 在`.bashrc`中添加

```
export PATH=$PATH:/usr/local/spark/bin
```

* 测试

```
source ~/.bashrc
spark-shell
```

按`Ctrl+D`退出



## Maven

在本地安装，自行配置代理。host上不安装。



## sbt

在本地安装。host上不安装。
