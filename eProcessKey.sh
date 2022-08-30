function eProcessKey(){    
    key="$1"
    # 如果输入为esc，强制回到终端模式并将命令序列置空
    [[ $key == $'\E' ]] && state=0 && exitseq=""
    # 文件偏移
    frow=$((rowofs + crow))
    fcol=$((colofs + ccol))
    case $state in 
    0) # 命令模式
        case $key in
        # 光标移动
            # 向上
            k|$'\E[A') #ARROWUP
                ((crow == 0)) && ((frow>0)) && ((rowofs-=1))
                ((crow > 0)) && ((crow-=1))
                ;;
            # 向下
            j|$'\E[B') #ARROWDOWN
                (($crow == $limitrows-1))&&((frow<numrows))&&((rowofs+=1)) 
                (($crow < $limitrows-1)) && ((crow+=1)) 
                ;;
            # 向右
            l|$'\E[C')
                (($ccol < $limitcols-1))&&((ccol+=1)) ;;
            # 向左
            h|$'\E[D')
                (($ccol > 0)) && ((ccol-=1)) ;;
            # 进入插入模式
            i)
                state=1;;
            # 进入命令模式·
            :)
                state=2
                exitseq+=':'
                ;;
            # 结束
            esac
        ;;

    1) #插入模式
        case $key in
        # 光标移动
            $'\E[A') #向上
                ((crow == 0)) && ((frow>0)) && ((rowofs-=1))
                ((crow > 0)) && ((crow-=1))
                ;;

            $'\E[B') #向下
                (($crow == $limitrows-1))&&((frow<numrows))&&((rowofs+=1)) 
                (($crow < $limitrows-1)) && ((crow+=1)) 
                ;;

            $'\E[C') #向右
                (($ccol < $limitcols-1))&&((ccol+=1)) ;;
            
            $'\E[D') #向左
                (($ccol > 0)) && ((ccol-=1)) ;;

            #换行
            $'\n') # enter
                # 取出该行
                var=${lines[$frow]}
                # 换行符分割左右
                ROW1=${var:0:${ccol}}
                ROW2=${var:${ccol}}
                # 行数加一
                ((numrows++))
                # 逐行下移
                for((i=$numrows-2; i > $frow; i--))
                do
                    lines[(($i + 1))]="${lines[$i]}"
                done
                # 换行符分割的两行分别赋值
                lines[(($frow + 1))]=$ROW2
                lines[$frow]=$ROW1
                ((ccol=0))
                ((crow++))
                ;;
            $'\E[3~') # 删除字符（DEL键删除光标右侧字符）
                lines[$frow]=${lines[$frow]:0:ccol}${lines[$frow]:ccol+1}
                ;;
            $'\177') # 删除字符（Backspace键删除光标右侧字符）
            # 实现方法，光标前移一位并调用DEL
            # 判断能否前移
            if [ $fcol -ne 0 ];then
                lines[$frow]=${lines[$frow]:0:ccol-1}${lines[$frow]:ccol}
                ((ccol--))
            # 如果不是文件头，那么能前移或者上移
            elif [ $frow -ne 0  ];then
                lines[$frow-1]=${lines[$frow-1]}${lines[$frow]}
            # 这种情况属于删换行符，需要行数减1并且拼接字符串
                for((i=$frow; i < $numrows - 1; i++))
                do
                    lines[$i]=${lines[(($i+1))]}
                done
                (($numrows--))
            # 将最后一个数组置零，再次确保渲染正确
                unset lines[-1]
            fi
                ;;
            # 默认字符：插入文本
            *)
                lines[$frow]=${lines[$frow]:0:ccol}$key${lines[$frow]:ccol}
                ;;
            esac
        ;;
    2) #命令模式
        case $key in
            # 输入回车时，检查命令序列
            $'\n') # enter
            # 如果是:wq，保存退出
            if [[ $exitseq == ":wq" ]];then
                count=0
                # 清空文件
                cat /dev/null > $filename 
                while ((count < numrows)); do
                    echo ${lines[$count]} >> $filename
                    (( count++ ))
                done
                exit
            fi
            # 如果是:q，直接退出
            if [[ $exitseq == ":q" ]];then
                exit
            fi
            ;;
            # 如果是正常字符，加入到命令序列
            *)
            exitseq+=${key};;
            esac
        ;;
    esac
}