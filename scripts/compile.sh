#!/bin/bash

scrp_path=$(cd `dirname $0`; pwd)
. ${scrp_path}/utils.sh

################################################################

THIS_PATH="scripts/compile.sh"

show_usage() {
    appname=$0
    echo_info "Usage: ${appname} [Options]"
    echo_info "     [NO OPTION]    Compile all."
    echo_info "     java           Compile jar files locally."
    echo_info "     dma           Compile dma files on servers."
    echo_info "     copy           Send compiled jar files to servers."
    echo_info "     shm            Configure shared memory on servers."
    echo_info "     clean          Clean generated files."
}

# 1
compile_java() {
    all_args=($*)
    if [ $# -eq 0 ] || [ $(contains "dpu" ${all_args[@]}) == "y" ]; then
        echo_back "cd ${dpu_path}"
        echo_back "make"
    fi
    if [ $# -eq 0 ] || [ $(contains "base" ${all_args[@]}) == "y" ]; then
        echo_back "cd ${base_path}"
        echo_back "make"
    fi
    if [ $# -eq 0 ] || [ $(contains "app" ${all_args[@]}) == "y" ]; then
        echo_back "cd ${app_path}/reduceby"
        echo_back "sbt package"
        echo_back "cd ${app_path}/groupby"
        echo_back "sbt package"
    fi
    # if [ $# -eq 0 ] || [ $(contains "ipc" ${all_args[@]}) == "y" ]; then
    #     echo_back "cd ${dpu_path}/ipc"
    #     echo_back "make"
    # fi
}

# 2
compile_dma() {
    for worker_id in ${worker_ids[@]}
    do
        dst="${host_prefix}${worker_id}"
        # compile dma module on dpu host
        echo_back "ssh ${dst} 'mkdir -p ${remote_tar_path}'"
        echo_back "ssh ${dst} 'cd ${remote_proj_path}/SparkDPU/dma;mkdir -p bin;cd bin;export JAVA_HOME=${remote_java_home};cmake ..;make -j 4;cp --remove-destination ${remote_proj_path}/SparkDPU/dma/bin/libspark_dpu.so ${remote_tar_path}/libspark_dpu.so'"

        # compile dma module on dpu
        echo_back "ssh ${dst} 'ssh ${dpu_from_host} '\''cd ${dpu_proj_path}/SparkDPU/dma;mkdir -p bin;cd bin;export JAVA_HOME=${dpu_java_home};cmake ..;make -j 4;cp --remove-destination ${dpu_proj_path}/SparkDPU/dma/bin/dpu_test ${dpu_tar_path}/dpu_test'\'''"
    done
}

# 3
copy_java() {
    all_args=($*)
    for worker_id in ${worker_ids[@]}
    do
        dst="${host_prefix}${worker_id}"
        echo_back "ssh ${dst} 'mkdir -p ${remote_tar_path}'"
        if [ $# -eq 0 ] || [ $(contains "dpu" ${all_args[@]}) == "y" ]; then
            echo_back "scp ${dpu_path}/target/spark-dpu-1.0-for-spark-2.3.0-jar-with-dependencies.jar ${dst}:${remote_tar_path}"
        fi
        if [ $# -eq 0 ] || [ $(contains "base" ${all_args[@]}) == "y" ]; then
            echo_back "scp ${base_path}/target/spark-base-1.0-for-spark-2.3.0-jar-with-dependencies.jar ${dst}:${remote_tar_path}"
        fi
        if [ $# -eq 0 ] || [ $(contains "app" ${all_args[@]}) == "y" ]; then

            echo_back "scp ${app_path}/reduceby/target/scala-2.11/reduceby-word-count_2.11-1.0.jar ${dst}:${remote_tar_path}"
            echo_back "scp ${app_path}/groupby/target/scala-2.11/groupby-word-count_2.11-1.0.jar ${dst}:${remote_tar_path}"
        fi
        # if [ $# -eq 0 ] || [ $(contains "ipc" ${all_args[@]}) == "y" ]; then
        #     echo_back "scp ${dpu_path}/ipc/libspark_dpu.so ${dst}:${remote_tar_path}"
        # fi
    done
}

do_compile() {
    compile_java
    compile_dma
    copy_java
}

do_clean() {
    echo_back "rm -rf ${build_path}"
    for worker_id in ${worker_ids[@]}
    do
        dst="${host_prefix}${worker_id}"
        echo_back "ssh ${dst} 'rm -rf ${remote_tar_path}'"
    done
}

# echo_back "cmake .."
# echo_back "make"

if [ $# -eq 0 ]; then
    do_compile
else
    all_args=($*)
    inner_args=${all_args[*]:1:$#}
    case $1 in
        clean)
            do_clean
            ;;
        dma)
            compile_dma
            ;;
        java)
            compile_java ${inner_args}
            ;;
        copy)
            copy_java ${inner_args}
            ;;
        javacopy)
            compile_java ${inner_args}
            copy_java ${inner_args}
            ;;
        *)
            show_usage
            ;;
    esac
fi
