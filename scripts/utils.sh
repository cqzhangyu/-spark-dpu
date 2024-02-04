#!bin/bash

# Usage : 
#   scrp_path=$(cd `dirname $0`; pwd)
#   . ${scrp_path}/utils.sh

if [ ${__UTILS_SH__} ]; then
    exit 0
fi
__UTILS_SH__=1

################################################################

color_black="\033[1;30m"
color_red="\033[1;31m"
color_green="\033[1;32m"
color_yellow="\033[1;33m"
color_blue="\033[1;34m"
color_purple="\033[1;35m"
color_skyblue="\033[1;36m"
color_white="\033[1;37m"
color_reset="\033[0m"
echo_back() {
    cmdLog=${1}
    printf "[${color_green}EXEC${color_reset}] ${cmdLog}\n"
    eval ${cmdLog}
}
echo_info() {
    cmdLog=${1}
    printf "[${color_green}INFO${color_reset}] ${cmdLog}\n"
}
echo_warn() {
    cmdLog=${1}
    printf "[${color_yellow}WARN${color_reset}] ${cmdLog}\n"
}
echo_erro() {
    cmdLog=${1}
    printf "[${color_red}ERRO${color_reset}] ${cmdLog}\n"
}

stop_cmd() {
    cmdName=${1}
    echo_info "find ${cmdName} and terminate"
    for pid in `pgrep -f ${cmdName}`; do
        echo_back "sudo kill -15 $pid"
    done
}

contains() {
    local value=${1}
    for ((i=2;i <= $#;i++)) {
        if [ "${!i}" == "${value}" ]; then
            echo "y"
            return 0
        fi
    }
    echo "n"
    return 1
}
################################################################

username=`whoami`
# scrp_path=$(cd `dirname $0`; pwd)
if [ ${scrp_path} ]; then
    proj_path=$(cd ${scrp_path}; cd ..; pwd)
else
    proj_path=$(cd `dirname $0`; cd ..; pwd)
fi
# echo_info "Project path: ${proj_path}"
# echo_info "Build path${build_path}"
dpu_path=${proj_path}/SparkDPU
base_path=${proj_path}/SparkBase
app_path=${proj_path}/SparkApps
SPARK_HOME=/usr/local/spark
remote_proj_path=/home/${username}/code/spark-dpu/
remote_tar_path=/home/${username}/target
remote_net="192.168.200"
dsts=(pkudpuhost1 pkudpuhost2)
worker_ids=(1 2)
