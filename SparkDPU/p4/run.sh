#!/bin/bash

#################################################
NF_NAME="sparkaggr"
#################################################
color_green="\033[32m"
color_purple="\033[35m"
color_reset="\033[0m"
progress_bar()
{
    if [ ${1} -le 0 ]; then
        echo_error "para must > 0"
	exit 0
    fi
    bTool=''
    curTool=0
    intervalTool=1
    maxlenTool=50
    temptimeTool=$((${1}-1))
    intervalTool=`expr ${temptimeTool} / ${maxlenTool}` 
    intervalTool=$((${intervalTool}+1))
    barlenTool=$((${1}/${intervalTool}))
    while [ ${curTool} -le ${1} ] 
    do
        printf "${color_green}Progress: ${color_reset}[${color_green}%-${barlenTool}s${color_reset}] [%2d/%d] \r" "$bTool" "${curTool}" "${1}";
        bTool+=">"
        ((curTool=curTool+${intervalTool}))
        sleep ${intervalTool} 
    done
    echo ""
}

echo_back()
{
    cmdLog=${1}
    printf "${color_purple}Executing: ${color_reset}${cmdLog}\n"
    eval ${cmdLog}
}


echo_back "$SDE/run_switchd.sh -p ${NF_NAME} >/dev/null 2>&1 &"
progress_bar 20
echo_back "$SDE/run_p4_tests.sh -t ptf/ -p ${NF_NAME}" 
echo_back "kill `pgrep bf_switchd`"
