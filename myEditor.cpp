/*UTF-8 Code format*/

#include <iostream>
#include <termios.h>
#include <string>
#include <unistd.h>
#include <sys/ioctl.h>
#include <vector>
#include <fstream>
using namespace std;
static struct termios termios_Orig;   // 命令行原终端属性
static struct termios termios_Editor; //进入文本编辑界面后，要修改命令行属性
/*eRow: editor row, 保存每行的行号，元文本和用来渲染的文本*/
class eRow
{
public:
    eRow()
    {
        irow = -1;
        content = "";
        render = "";
    }
    int irow;       //行号
    string content; //元文本
    string render;  //渲染文本
};
class eText //保存所有的行文本信息
{
public:
    int numrows;      //一共有多少行
    vector<eRow> row; //每行信息
};
class eTerminalProperty //编辑器状态
{
public:
    unsigned int cx_, cy_;         //光标位置
    unsigned int limitrows_;       //行数限制
    unsigned int limitcols_;       //列数限制
    unsigned int rowofs_, colofs_; //偏移量
    unsigned int state_;           // 0：命令模式 1：插入模式
    unsigned int editmode_;        // 0：终端模式 1：编辑模式
    string filename_;              //文件名称
    string instrmsg_;              //帮助信息
    eText T;                       //文本
} P;

enum KEY_PRESS //按键ASCII和自定义按键
{
    ENTER = 13,
    ESC = 27,
    BACKSPACE = 127, // BACKSPACE删除左边
    LEFT = 256,      //键盘左
    RIGHT,           //键盘右
    UP,              //键盘上
    DOWN,            //键盘下
    PAGEUP,
    PAGEDOWN,
    DEL // DEL删除右边
};
/*Initialize*/
void eInit(string filename);
/*Terminal*/
int getWindowSize(unsigned int *rows, unsigned int *cols);
void disableEditorTerminal();
int enableEditorTerminal();
/*Input*/
int eReadKey();
void eProcKey();
/*Render*/
void eRender();
void eMoveCursor(int key);
void eAdjustCx(eRow *row);
/*Edit*/
void eLinefeed();
void eDelChar();
void eDelCharFront();
void eInputChar(int c);
/*File*/
int eReadFile();
int eSave();
/*初始化编辑器参数*/
void eInit(string filename)
{
    P.cx_ = P.cy_ = 0;                           //光标位置置零
    P.state_ = 0;                                //命令模式
    P.filename_ = filename;                      //文件名词
    P.T.numrows = 0;                             //文件行数
    P.T.row.resize(0);                           //文件文本
    P.editmode_ = 0;                             //终端模式
    getWindowSize(&P.limitrows_, &P.limitcols_); //获取窗口大小
    P.limitrows_ -= 2;                           //底下两行预留下来输出帮助信息
    eReadFile();                                 //读取文本
}
/*Terminal*/
/*获取窗口大小*/
int getWindowSize(unsigned int *rows, unsigned int *cols)
{
    /*winsize结构位于termios.h,用于获取终端的行数和列数*/
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        /*获取终端的行数和列数*/
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*将shell终端恢复原设置*/
void disableEditorTerminal()
{
    if (P.editmode_ == 1)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_Orig);
        P.editmode_ = 0;
    }
}

int enableEditorTerminal()
{
    tcgetattr(STDIN_FILENO, &termios_Orig);
    if (P.editmode_ == 1)
        return 0;
    /*atexit: 注册当exit()被调用时，在其之前调用的函数*/
    atexit(disableEditorTerminal);
    termios_Editor = termios_Orig;
    /*命令行参数设置*/
    termios_Editor.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    termios_Editor.c_oflag &= ~(OPOST);
    termios_Editor.c_cflag |= (CS8);
    /*关键参数：取消回显，取消扩充函数，取消SIG信号：ctrl-z，ctrl-c*/
    termios_Editor.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    termios_Editor.c_cc[VMIN] = 0;  /* 输入符号，如果超过100ms时延，输入0 */
    termios_Editor.c_cc[VTIME] = 1; /* 100 ms 时延 */
    //设置为当前的命令参数
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_Editor);
    P.editmode_ = 1;
    return 0;
}
/*Input*/

int eReadKey()
{
    int nread;
    char c, seq[3];
    while ((nread = read(STDIN_FILENO, &c, 1)) == 0)
        ;
    if (nread == -1)
        exit(1);

    if (c == ESC)
    {
        if (read(STDIN_FILENO, seq, 1) == 0)
            return ESC;
        if (read(STDIN_FILENO, seq + 1, 1) == 0)
            return ESC;

        /* ESC [ sequences. */
        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, seq + 2, 1) == 0)
                    return ESC;
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '3':
                        return DEL;
                    case '5':
                        return PAGEUP;
                    case '6':
                        return PAGEDOWN;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return UP;
                case 'B':
                    return DOWN;
                case 'C':
                    return RIGHT;
                case 'D':
                    return LEFT;
                }
            }
        }
        return ESC;
    }
    else
        return c;
}

void eProcKey()
{
    int c = eReadKey();
    switch (c)
    {

    case ENTER:
        if (P.state_ == 1)
            eLinefeed();
        break;
    case BACKSPACE:
        if (P.state_ == 1)
            eDelCharFront();
        break;
    case DEL:
        if (P.state_ == 1)
            eDelChar();
        break;
    case PAGEUP:
    case PAGEDOWN:
        if (c == PAGEUP && P.cy_ != 0)
            P.cy_ = 0;
        else if (c == PAGEDOWN && P.cy_ != P.limitrows_ - 1)
            P.cy_ = P.limitrows_ - 1;
        for (unsigned int i = 0; i < P.limitrows_ - 1; i++)
            eMoveCursor(c == PAGEUP ? UP : DOWN);
        break;

    case UP:
    case DOWN:
    case LEFT:
    case RIGHT:
        eMoveCursor(c);
        break;
    case ESC:
        if (P.state_ == 1)
            P.state_ = 0;
        break;
    case 'i':
        if (P.state_ == 0)
        {
            P.state_ = 1;
            break;
        }
    case 'q':
        if (P.state_ == 0)
        {
            eSave();
            exit(0);
        }
    default:
        if (P.state_ == 1)
            eInputChar(c);
        break;
    }
}
/*Render*/
void eRender()
{
    /*?25l隐藏光标，H放置光标(默认第一行第一列)*/
    string txtbuf = "\x1b[?25l\x1b[H";

    for (unsigned int y = 0; y < P.limitrows_; y++)
    {
        int row_proc_ind = P.rowofs_ + y;

        if (row_proc_ind < P.T.numrows)
        {
            eRow row_proc = P.T.row[row_proc_ind];
            if (row_proc.content.length() > P.limitcols_)
            {
                row_proc.render = row_proc.content.substr(P.colofs_, P.limitcols_);
            }
            else
            {
                row_proc.render = row_proc.content;
            }
            txtbuf += row_proc.render;
            txtbuf += "\x1b[0K\r\n";
        }
        else
        {
            txtbuf += "~\x1b[0K\r\n";
        }
    }
    if (P.state_ == 0)
    {
        txtbuf += "命令模式";
        txtbuf += "\x1b[0K\r\n";
    }
    else
    {
        txtbuf += "编辑模式";
        txtbuf += "\x1b[0K\r\n";
    }
    char buf[100];
    /*H参数从1开始，放置光标到应有的位置*/
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", P.cy_ + 1, P.cx_ + 1);
    txtbuf += buf;
    txtbuf += "\x1b[?25h"; //显示光标
    write(STDOUT_FILENO, txtbuf.c_str(), txtbuf.length());
}
void eMoveCursor(int key)
{
    unsigned int fy = P.rowofs_ + P.cy_;
    unsigned int fx = P.colofs_ + P.cx_;
    eRow *row = (fy >= P.T.numrows) ? NULL : &P.T.row[fy];
    /*Test*/
    // if (row == NULL)
    //     exit(0);
    if (key == LEFT)
    {
        if (P.cx_ > 0)
        {
            P.cx_--;
        }
        else if (P.colofs_ > 0)
        {
            P.colofs_--;
        }
        else // P.cx == 0
        {
            if (fy > 0)
            {
                if (P.cy_ > 0)
                {
                    P.cy_--;
                }
                else
                {
                    P.rowofs_--;
                }
                eRow r = P.T.row[fy - 1];
                P.cx_ = (r.content.length() > P.limitcols_ - 1) ? P.limitcols_ - 1 : r.content.length();
                P.colofs_ = (r.content.length() > P.limitcols_ - 1) ? r.content.length() - P.cx_ : 0;
            }
        }
    }
    else if (key == RIGHT)
    {
        if (row != NULL && fx < row->content.length())
        {
            if (P.cx_ == P.limitcols_ - 1)
            {
                P.colofs_++;
            }
            else
            {
                P.cx_ += 1;
            }
        }
        else if (row && fx == row->content.length())
        {
            P.cx_ = 0;
            P.colofs_ = 0;
            if (P.cy_ == P.limitrows_ - 1)
            {
                P.rowofs_++;
            }
            else
            {
                P.cy_ += 1;
            }
        }
    }
    else if (key == UP)
    {
        if (P.cy_ == 0)
        {
            if (P.rowofs_)
                P.rowofs_--;
        }
        else
        {
            P.cy_ -= 1;
        }
    }
    else if (key == DOWN)
    {
        if (fy < P.T.numrows)
        {
            if (P.cy_ == P.limitrows_ - 1)
            {
                P.rowofs_++;
            }
            else
            {
                P.cy_ += 1;
            }
        }
    }
    eAdjustCx(row);
}
void eAdjustCx(eRow *row)
{
    unsigned int fy = P.rowofs_ + P.cy_;
    unsigned int fx = P.colofs_ + P.cx_;
    row = (fy >= P.T.numrows) ? NULL : &P.T.row[fy];
    int rowlen = row ? row->content.length() : 0;
    if (fx > rowlen)
    {
        P.cx_ -= fx - rowlen;
        if (P.cx_ < 0)
        {
            P.colofs_ += P.cx_;
            P.cx_ = 0;
        }
    }
}
void eLinefeed()
{
    eRow a;
    eRow b;
    unsigned int fx = P.cx_ + P.colofs_;
    unsigned int fy = P.cy_ + P.rowofs_;
    eRow &row = P.T.row[fy];
    a.content = row.content.substr(0, fx);
    b.content = row.content.substr(fx, row.content.length() - fx);
    a.irow = fy;
    b.irow = fy + 1;
    P.T.numrows++;
    P.T.row.resize(P.T.numrows);
    for (int i = P.T.numrows - 2; i > fy; i--)
    {
        P.T.row[i + 1] = P.T.row[i];
        P.T.row[i + 1].irow = i + 1;
    }
    P.T.row[fy] = a;
    P.T.row[fy + 1] = b;
}
void eInputChar(int c)
{
    unsigned int fx = P.cx_ + P.colofs_;
    unsigned int fy = P.cy_ + P.rowofs_;
    eRow &row = P.T.row[fy];
    row.content.insert(fy, 1, c);
}
void eDelCharFront()
{

    unsigned int fx = P.cx_ + P.colofs_;
    if (fx >= 0)
    {
        unsigned int fy = P.cy_ + P.rowofs_;
        eRow &row = P.T.row[fy];
        if (fx != 0 || fy != 0)
        {
            eMoveCursor(LEFT);
            eDelChar();
        }
    }
}
void eDelChar()
{
    unsigned int fx = P.cx_ + P.colofs_;
    unsigned int fy = P.cy_ + P.rowofs_;
    eRow &row = P.T.row[fy];
    if (fx != row.content.length())
        row.content.erase(fx, 1);
    else
    {
        eRow row_next;
        if (fy != P.T.numrows - 1)
            row_next = P.T.row[fy + 1];
        string row_content_new = row.content + row_next.content;
        for (int i = fy; i < P.T.numrows - 1; i++)
        {
            P.T.row[i] = P.T.row[i + 1];
            P.T.row[i].irow = i;
        }
        P.T.row[fy].content = row_content_new;
        P.T.numrows--;
    }
}

int eReadFile()
{
    FILE *fp = fopen(P.filename_.c_str(), "r");

    if (!fp)
        cerr << "No such file!";

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {

        if (linelen && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            line[--linelen] = '\0';
        string *line_str = new string(line);
        P.T.row.resize(P.T.numrows + 1);
        P.T.row[P.T.numrows].irow = P.T.numrows;
        P.T.row[P.T.numrows].content = *line_str;
        P.T.row[P.T.numrows].render = "";
        P.T.numrows++;
    }
    free(line);
    fclose(fp);
    return 0;
}
int eSave()
{
    ofstream os;
    os.open(P.filename_, ios::out | ios::trunc);
    for (int i = 0; i < P.T.numrows; i++)
    {
        os << P.T.row[i].content;
        os << "\n";
    }
    os.close();
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        cerr << "program <filename>";
    }
    eInit(argv[1]);
    // testRow();
    enableEditorTerminal();
    while (true)
    {
        eRender();
        eProcKey();
    }
    return 0;
}