# DPU 环境配置

## CMake

去官网下载cmake，注意下载**arm**版的！

```bash
sudo mv cmake-3.28.2-linux-aarch64 /usr/local

sudo ln -s /usr/local/cmake-3.28.2-linux-aarch64/bin/cmake /usr/bin/cmake
```





## java

去官网下载java8，arm版

[Java Downloads | Oracle](https://www.oracle.com/java/technologies/downloads/#java8)

```bash
tar xvf ~/jdk-8u401-linux-aarch64.tar.gz
sudo mv jdk1.8.0_401 /usr/local
```

记住这个java路径，在编译JNI的时候要用
