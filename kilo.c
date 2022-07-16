#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h> // get terminal window size
#include <termios.h>
#include <unistd.h>
/*MACRO*/

/*
    ctrl + char a-z maps to 1-26
*/
#define CTRL_KEY(k) ((k)&0x1f)
#define KILO_VERSION "0.0.1"
#define ABUF_INIT \
    {             \
        NULL, 0   \
    }

/*STRUCT*/

/*** append buffer ***/
//写入终端的字符缓存
typedef struct erow
{
    int size;
    char *chars;
} erow;

struct abuf
{
    char *b;
    int len;
};

enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

/*global config*/
struct editorConfig
{
    int cx, cy;
    int screenrows;              //终端行数
    int screencols;              //终端列数
    struct termios orig_termios; //终端属性
    int numrows;
    erow row;
};
struct editorConfig E;

/*File I/O*/
void editorOpen()
{
    char *line = "Hello, world!";
    ssize_t linelen = 13;
    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1;
}
/*Append buffer*/
void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void abFree(struct abuf *ab)
{
    free(ab->b);
}

/*Terminal*/
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode()
{
    /*
        TCSAFLUSH: discards any unread input before applying the changes to the terminal.
        chinese: 如果有没有被读的输入，直接丢弃
        orig_termos：保存原有的命令行状态
    */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
void enableRawMode()
{
    /*
      save original termios for recovery
      chinese: 保存原有的命令行状态，以便程序退出后恢复
    */
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    /*
      When exit the program, automatically call disableRawMode
      chinese: 把disableRawMode注册，当程序退出时自动调用
    */
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    /*
      discard ECHO: the input won't echo in shell, or will destroy our format
      discard ICANON: reading input byte-by-byte, instead of line-by-line.
      disable ctrl-S, ctrl-Q and ctrl-V using other bits
      chinese:
      取消ECHO位用来取消回显，否则想象一下，命令行的文本编辑会乱七八糟
      取消ICANON: 不用等待回车指定逐行输入，而是每一个字符都单独输入
    */
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // raw.c_cc[VMIN] = 0;
    // raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

/*Render*/

void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        if (y == E.screenrows / 3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols)
                welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--)
                abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        }
        else
        {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen()
{
    /*
        转义序列(VT100):
        (27)[2J
        转义序列控制终端做各种命令
        “2”：清除整片屏幕
        “J”：清除命令
        "l": 光标隐藏
        "h": 光标显示
        “H”: {row_num};{col_num}放置光标的位置
    */
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
        {
            E.cx--;
        }
        break;
    case ARROW_RIGHT:
        if (E.cx != E.screencols - 1)
        {
            E.cx++;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy != E.screenrows - 1)
        {
            E.cy++;
        }
        break;
    }
}
int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    /*
        n command: 需要argument 6 来 查询光标位置
    */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    /*
        查询返回结果：(esc){row_num};{col_num}R
    */
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    /*
        获取窗口大小异常：并非所有系统都适用ioctl
    */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    /*
        \x1b[999C: 把光标移到最右
        \x1b[999B: 把光标移到最下
    */
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    }
    /*
        如果能够获得窗口大小
    */
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;

        return 0;
    }
}
/*Input*/
int editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                }
            }
        }
        return '\x1b';
    }
    else
    {
        return c;
    }
}
void editorProcessKeypress()
{
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    {
        die("getWindowSize");
    }
}

/*** init ***/
int main()
{
    enableRawMode();
    initEditor();
    editorOpen();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}