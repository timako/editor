function eRender(){
    # 清屏
    clear
    # 将光标移动到左上角
    tput cup 0 0
    # 渲染的起始点
    count=rowofs
    # 逐行输出
    while ((count < rowofs + limitrows-1))&&((count < numrows)); do
        echo ${lines[$count]}
        (( count++ ))
    done
    # 光标移动到下方
    # 根据状态输出模式
    tput cup $(($limitrows)) 0
    (($state==0))&&echo "终端模式"
    (($state==1))&&echo "插入模式"
    (($state==2))&&echo "命令模式"
    # 如果是命令模式，额外在下方输出命令序列
    if [ $state -eq 2 ]; then
        tput cup $(($limitrows+1)) 0
        echo $exitseq
    fi
    tput cup 0 0
    # 将光标移动到要插入字符的位置
    tput cup $crow $ccol
}