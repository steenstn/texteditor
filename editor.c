// ASCII table https://www.asciitable.com/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct editorrow {
  char *chars;
  int length;
} editorrow;

struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
  editorrow *row;
  int numRows;
  int cursor_x;
  int cursor_y;
  char status_row[50];
};

struct editorConfig E;

enum editorKey { ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN };
void die(const char *s) {
  perror(s);
  exit(1);
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void initEditor(void) {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
    die("getWindowSize");
  }
  E.numRows = 0;
  E.row = NULL;
  E.cursor_x = 0;
  E.cursor_y = 0;
}

void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enableRawMode(void) {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;

  // Turn off sofrware control flow (ctrl s, ctrl q)
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_cflag |= (CS8);
  // Turn of output processing
  raw.c_oflag &= ~(OPOST);
  // Turn off echo, canon mode, sigint signals, ctrl+v
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // Min number of bytes of input needed before read can return
  // Set to 0 so read() can return as soon as there is any input to read
  raw.c_cc[VMIN] = 0;

  // max amount of time to wait before read returns (tenths of a second)
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}
int editorReadKey(void) {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      switch (seq[1]) {
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

    return '\x1b';
  }

  return c;
}

void editorMoveCursor(int key) {
  switch (key) {
  case ARROW_LEFT:
    E.cursor_x--;
    break;
  case ARROW_RIGHT:
    E.cursor_x++;
    break;
  case ARROW_UP:
    E.cursor_y--;
    break;
  case ARROW_DOWN:
    E.cursor_y++;
    break;
  }
}
void editorProcessKeypress(void) {
  int c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    exit(0);
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
  snprintf(E.status_row, 10, "%d\r\n", E.cursor_x); // printf("escape!");
  if (c == '\x1b') {
    char *lol = "esc\r\n";
    write(STDOUT_FILENO, lol, 5); // printf("escape!");
  }
}

void editorDrawRows(void) {
  for (int i = 0; i < E.numRows; i++) {
    write(STDOUT_FILENO, E.row[i].chars, E.row[i].length);
    write(STDOUT_FILENO, "\r\n", 2);
  }
  write(STDOUT_FILENO, E.status_row, 10);
  //  write(STDOUT_FILENO, E.row.chars, E.row.length);
  /*  for (int y = 0; y < E.screenrows; y++) {
      write(STDOUT_FILENO, "*\r\n", 3);
    }*/
}

void editorRefreshScreen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3); // Move cursor to top left

  editorDrawRows();

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cursor_y + 1, E.cursor_x + 1);
  write(STDOUT_FILENO, buf, strlen(buf));
}

void loadFile(char *filename) {
  FILE *fp;
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  fp = fopen(filename, "r");
  if (fp == NULL) {
    die("loadFile");
  }

  while ((linelen = getline(&line, &linecap, fp)) != -1) {

    //    printf("Got line fo length %zu: \r\n", read);
    //  printf("%s", line);
    while (linelen > 0 &&
           (line[linelen - 1] == '\r' || line[linelen - 1] == '\n')) {
      linelen--;
    }
    E.row = realloc(E.row, sizeof(editorrow) * (E.numRows + 1));
    int index = E.numRows;
    E.row[index].length = linelen;
    E.row[index].chars = malloc(linelen + 1);
    memcpy(E.row[index].chars, line, linelen);
    E.row[index].chars[linelen] = '\0';
    E.numRows++;
  }

  free(line);
  fclose(fp);
}

int main(void) {
  enableRawMode();
  initEditor();
  loadFile("test.txt");
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
