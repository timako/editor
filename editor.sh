#UTF-8 format

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    exit
fi
# 光标偏移
crow=0
ccol=0
# 终端偏移
rowofs=0
colofs=0
state=0 #0：命令模式 1：插入模式
editmode=0 #0：终端模式 1：编辑模式
limitrows=$(($(tput lines)-2))
limitcols=$(tput cols)

exitseq="" 
##############   初始化结束   ##############

# 读取文件(对应eReadFile)
filename=$1
IFS=$'\n'
index=0
# 用数组保存所有文本
while read line ; do
        lines[$index]="$line"
        index=$(($index+1))
done < $1

numrows=$(cat $1 | wc -l)
##############   读文件结束  ##############

source ./eProcessKey.sh
source ./eRender.sh

# main
eRender
while read -sN1 iseq #输入序列
do

#eReadKey
# 读取输入序列：因为关于控制符号的案件是以最高三位的序列输入的，
# 这里需要连续读取三位，设置0.0001的限时保证不会错误读取到下一个输入
read -sN1 -t 0.0001 inputarr1
read -sN1 -t 0.0001 inputarr2
read -sN1 -t 0.0001 inputarr3

iseq+=${inputarr1}
iseq+=${inputarr2}
iseq+=${inputarr3}

# 调用函数处理输入序列
eProcessKey "$iseq"
# 渲染终端
eRender
done