/*
 * Milo v3 -- Flattened Data Structures.
 * * Strategy:
 * 1. Replaced 'struct editorConfig' with individual global variables.
 * 2. Replaced 'struct erow' array with parallel arrays (int* and char**).
 * 3. Removed all struct dependencies to bypass compiler lookup errors.
 */

/* ==================== EXTERNAL DECLARATIONS ==================== */

extern int open(const char *pathname, int flags);
extern int close(int fd);
extern long read(int fd, void *buf, unsigned long count);
extern long write(int fd, const void *buf, unsigned long count);
extern int snprintf(char *str, unsigned long size, const char *format, ...);
extern int system(const char *command);
extern void *malloc(unsigned long size);
extern void *realloc(void *ptr, unsigned long size);
extern void free(void *ptr);
extern void *memcpy(void *dest, const void *src, unsigned long n);
extern unsigned long strlen(const char *s);
extern void exit(int status);

/* ==================== GLOBALS (Flattened EditorConfig) ==================== */

/* Cursor Position */
int E_cx;
int E_cy;

/* Viewport Offsets */
int E_rowoff;
int E_coloff;

/* Screen Dimensions */
int E_screenrows;
int E_screencols;

/* Row Buffer (Parallel Arrays instead of struct erow) */
int E_numrows;
int *E_row_size;    /* Array of row sizes */
char **E_row_chars; /* Array of row content strings */

/* Status */
int E_dirty;
char *E_filename;

/* Constants */
const int KEY_CTRL_Q = 17;
const int KEY_ARROW_LEFT = 1000;
const int KEY_ARROW_RIGHT = 1001;
const int KEY_ARROW_UP = 1002;
const int KEY_ARROW_DOWN = 1003;
const int KEY_HOME = 1004;
const int KEY_END = 1005;
const int KEY_PAGE_UP = 1006;
const int KEY_PAGE_DOWN = 1007;
const int KEY_DEL = 1008;

const int MY_O_RDWR = 2;

/* ==================== TERMINAL HANDLING ==================== */

void enableRawMode()
{
	system("stty raw -echo");
}

void disableRawMode()
{
	system("stty sane");
}

int getWindowSize(int *rows, int *cols)
{
	int ret;
	char buf[32];
	char *p;
	unsigned int i;

	ret = write(1, "\x1b[999C\x1b[999B", 12);
	if (ret != 12)
		return -1;

	ret = write(1, "\x1b[6n", 4);
	if (ret != 4)
		return -1;

	i = 0;
	while (i < 31) {
		if (read(0, buf + i, 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != 27)
		return -1;
	if (buf[1] != '[')
		return -1;

	p = buf + 2;
	*rows = 0;
	while (*p >= '0' && *p <= '9') {
		*rows = (*rows * 10) + (*p - '0');
		p++;
	}
	if (*p == ';')
		p++;
	*cols = 0;
	while (*p >= '0' && *p <= '9') {
		*cols = (*cols * 10) + (*p - '0');
		p++;
	}
	return 0;
}

/* ==================== ROW OPERATIONS ==================== */

void editorAppendRow(char *s, int len)
{
	/* Realloc parallel arrays */
	/* sizeof(int) is usually 4, sizeof(char*) is 8 (64bit) or 4 (32bit) */
	/* Using hardcoded sizes for safety if sizeof is flaky, assuming 64-bit ptr */

	int new_size_int;
	int new_size_ptr;

	/* 4 bytes for int, 8 bytes for pointer */
	new_size_int = (E_numrows + 1) * 4;
	new_size_ptr = (E_numrows + 1) * 8;

	E_row_size = realloc(E_row_size, new_size_int);
	E_row_chars = realloc(E_row_chars, new_size_ptr);

	/* Set data at index */
	E_row_size[E_numrows] = len;
	E_row_chars[E_numrows] = malloc(len + 1);
	memcpy(E_row_chars[E_numrows], s, len);
	E_row_chars[E_numrows][len] = '\0';

	E_numrows++;
}

/* ==================== INPUT ==================== */

int editorReadKey()
{
	int nread;
	char c;
	char seq[3];

	while ((nread = read(0, &c, 1)) == 0)
		;

	if (nread == -1) {
		exit(1);
		return 0;
	}

	if (c == 27) {
		if (read(0, seq, 1) == 0)
			return 27;
		if (read(0, seq + 1, 1) == 0)
			return 27;

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(0, seq + 2, 1) == 0)
					return 27;
				if (seq[2] == '~') {
					if (seq[1] == '3')
						return KEY_DEL;
					if (seq[1] == '5')
						return KEY_PAGE_UP;
					if (seq[1] == '6')
						return KEY_PAGE_DOWN;
				}
			} else {
				if (seq[1] == 'A')
					return KEY_ARROW_UP;
				if (seq[1] == 'B')
					return KEY_ARROW_DOWN;
				if (seq[1] == 'C')
					return KEY_ARROW_RIGHT;
				if (seq[1] == 'D')
					return KEY_ARROW_LEFT;
				if (seq[1] == 'H')
					return KEY_HOME;
				if (seq[1] == 'F')
					return KEY_END;
			}
		}
		return 27;
	}
	return c;
}

void editorMoveCursor(int key)
{
	int rowlen;

	if (key == KEY_ARROW_LEFT) {
		if (E_cx != 0)
			E_cx--;
	} else if (key == KEY_ARROW_RIGHT) {
		if (E_cx < E_screencols - 1)
			E_cx++;
	} else if (key == KEY_ARROW_UP) {
		if (E_cy != 0)
			E_cy--;
	} else if (key == KEY_ARROW_DOWN) {
		if (E_cy < E_numrows)
			E_cy++;
	}

	/* Snap to end of line */
	rowlen = 0;
	if (E_cy < E_numrows) {
		rowlen = E_row_size[E_cy];
	}

	if (E_cx > rowlen)
		E_cx = rowlen;
}

void editorProcessKeypress()
{
	int c;
	c = editorReadKey();

	if (c == KEY_CTRL_Q) {
		write(1, "\x1b[2J", 4);
		write(1, "\x1b[H", 3);
		disableRawMode();
		exit(0);
		return;
	}

	if (c == KEY_ARROW_UP || c == KEY_ARROW_DOWN || c == KEY_ARROW_LEFT || c == KEY_ARROW_RIGHT) {
		editorMoveCursor(c);
		return;
	}
}

/* ==================== OUTPUT ==================== */

/* Manual buffer handling without struct */
char *ab_b;
int ab_len;

void abAppend(const char *s, int len)
{
	char *new_ptr;
	new_ptr = realloc(ab_b, ab_len + len);

	if (new_ptr == 0)
		return;

	memcpy(new_ptr + ab_len, s, len);
	ab_b = new_ptr;
	ab_len = ab_len + len;
}

void editorRefreshScreen()
{
	int y;
	int filerow;
	int len;
	char buf[32];
	char welcome[80];
	int welcomelen;
	int padding;

	/* Initialize append buffer globals */
	ab_b = 0;
	ab_len = 0;

	abAppend("\x1b[?25l", 6);
	abAppend("\x1b[H", 3);

	for (y = 0; y < E_screenrows; y++) {
		filerow = E_rowoff + y;

		if (filerow >= E_numrows) {
			if (E_numrows == 0 && y == E_screenrows / 3) {
				welcomelen = snprintf(welcome, 80, "Milo Editor v3");
				if (welcomelen > E_screencols)
					welcomelen = E_screencols;

				padding = (E_screencols - welcomelen) / 2;
				if (padding > 0) {
					abAppend("~", 1);
					padding--;
				}
				while (padding-- > 0)
					abAppend(" ", 1);
				abAppend(welcome, welcomelen);
			} else {
				abAppend("~", 1);
			}
		} else {
			len = E_row_size[filerow] - E_coloff;
			if (len < 0)
				len = 0;
			if (len > E_screencols)
				len = E_screencols;

			if (len > 0) {
				/* Access parallel char array */
				abAppend(E_row_chars[filerow] + E_coloff, len);
			}
		}

		abAppend("\x1b[K", 3);
		if (y < E_screenrows - 1) {
			abAppend("\r\n", 2);
		}
	}

	snprintf(buf, 32, "\x1b[%d;%dH", (E_cy - E_rowoff) + 1, (E_cx - E_coloff) + 1);
	abAppend(buf, strlen(buf));

	abAppend("\x1b[?25h", 6);

	write(1, ab_b, ab_len);
	free(ab_b);
}

/* ==================== INIT ==================== */

void initEditor()
{
	int rows;
	int cols;

	E_cx = 0;
	E_cy = 0;
	E_rowoff = 0;
	E_coloff = 0;
	E_numrows = 0;
	E_row_size = 0;
	E_row_chars = 0;
	E_dirty = 0;
	E_filename = 0;

	if (getWindowSize(&rows, &cols) == -1) {
		exit(1);
	}
	E_screenrows = rows;
	E_screencols = cols;
}

int main(int argc, char **argv)
{
	int fd;

	enableRawMode();
	initEditor();

	if (argc >= 2) {
		E_filename = argv[1];
		fd = open(argv[1], MY_O_RDWR);
		if (fd != -1) {
			/* Loading logic stub */
			editorAppendRow("Hello from Milo v3", 18);
			editorAppendRow("No structs here!", 16);
		}
	}

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}
