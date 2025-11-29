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

/* Cursor Position */
int e_cx;
int e_cy;

/* Viewport Offsets */
int e_rowoff;
int e_coloff;

/* Screen Dimensions */
int e_screenrows;
int e_screencols;

/* Row Buffer (Parallel Arrays instead of struct erow) */
int e_numrows;
int *e_row_size;    /* Array of row sizes */
char **e_row_chars; /* Array of row content strings */

/* Status */
int e_dirty;
char *e_filename;

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

void enable_raw_mode()
{
	system("stty raw -echo");
}

void disable_raw_mode()
{
	system("stty sane");
}

int get_window_size(int *rows, int *cols)
{
	int ret;
	char buf[32];
	char *p;
	unsigned int i;

	ret = write(1, "\x1b[999C\x1b[999B", 12);
	if (ret != 12) {
		return -1;
	}

	ret = write(1, "\x1b[6n", 4);
	if (ret != 4) {
		return -1;
	}

	i = 0;
	while (i < 31) {
		if (read(0, buf + i, 1) != 1) {
			break;
		}
		if (buf[i] == 'R') {
			break;
		}
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != 27) {
		return -1;
	}
	if (buf[1] != '[') {
		return -1;
	}

	p = buf + 2;
	*rows = 0;
	while (*p >= '0' && *p <= '9') {
		*rows = (*rows * 10) + (*p - '0');
		p++;
	}
	if (*p == ';') {
		p++;
	}
	*cols = 0;
	while (*p >= '0' && *p <= '9') {
		*cols = (*cols * 10) + (*p - '0');
		p++;
	}
	return 0;
}

void editor_append_row(char *s, int len)
{
	/* Realloc parallel arrays */
	/* sizeof(int) is usually 4, sizeof(char*) is 8 (64bit) or 4 (32bit) */
	/* Using hardcoded sizes for safety if sizeof is flaky, assuming 64-bit ptr */

	int new_size_int;
	int new_size_ptr;

	/* 4 bytes for int, 8 bytes for pointer */
	new_size_int = (e_numrows + 1) * 4;
	new_size_ptr = (e_numrows + 1) * 8;

	e_row_size = realloc(e_row_size, new_size_int);
	e_row_chars = realloc(e_row_chars, new_size_ptr);

	/* Set data at index */
	e_row_size[e_numrows] = len;
	e_row_chars[e_numrows] = malloc(len + 1);
	memcpy(e_row_chars[e_numrows], s, len);
	e_row_chars[e_numrows][len] = '\0';

	e_numrows++;
}

int editor_read_key()
{
	int nread;
	char c;
	char seq[3];

	while ((nread = read(0, &c, 1)) == 0) {
		;
	}

	if (nread == -1) {
		exit(1);
		return 0;
	}

	if (c == 27) {
		if (read(0, seq, 1) == 0) {
			return 27;
		}
		if (read(0, seq + 1, 1) == 0) {
			return 27;
		}

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(0, seq + 2, 1) == 0) {
					return 27;
				}
				if (seq[2] == '~') {
					if (seq[1] == '3') {
						return KEY_DEL;
					}
					if (seq[1] == '5') {
						return KEY_PAGE_UP;
					}
					if (seq[1] == '6') {
						return KEY_PAGE_DOWN;
					}
				}
			} else {
				if (seq[1] == 'A') {
					return KEY_ARROW_UP;
				}
				if (seq[1] == 'B') {
					return KEY_ARROW_DOWN;
				}
				if (seq[1] == 'C') {
					return KEY_ARROW_RIGHT;
				}
				if (seq[1] == 'D') {
					return KEY_ARROW_LEFT;
				}
				if (seq[1] == 'H') {
					return KEY_HOME;
				}
				if (seq[1] == 'F') {
					return KEY_END;
				}
			}
		}
		return 27;
	}
	return c;
}

void editor_move_cursor(int key)
{
	int rowlen;

	if (key == KEY_ARROW_LEFT) {
		if (e_cx != 0) {
			e_cx--;
		}
	} else if (key == KEY_ARROW_RIGHT) {
		if (e_cx < e_screencols - 1) {
			e_cx++;
		}
	} else if (key == KEY_ARROW_UP) {
		if (e_cy != 0) {
			e_cy--;
		}
	} else if (key == KEY_ARROW_DOWN) {
		if (e_cy < e_numrows) {
			e_cy++;
		}
	}

	rowlen = 0;
	if (e_cy < e_numrows) {
		rowlen = e_row_size[e_cy];
	}

	if (e_cx > rowlen) {
		e_cx = rowlen;
	}
}

void editor_process_keypress()
{
	int c;
	c = editor_read_key();

	if (c == KEY_CTRL_Q) {
		write(1, "\x1b[2J", 4);
		write(1, "\x1b[H", 3);
		disable_raw_mode();
		exit(0);
		return;
	}

	if (c == KEY_ARROW_UP || c == KEY_ARROW_DOWN || c == KEY_ARROW_LEFT || c == KEY_ARROW_RIGHT) {
		editor_move_cursor(c);
		return;
	}
}

char *ab_b;
int ab_len;

void ab_append(const char *s, int len)
{
	char *new_ptr;
	new_ptr = realloc(ab_b, ab_len + len);

	if (new_ptr == 0) {
		return;
	}

	memcpy(new_ptr + ab_len, s, len);
	ab_b = new_ptr;
	ab_len = ab_len + len;
}

void editor_refresh_screen()
{
	int y;
	int filerow;
	int len;
	char buf[32];
	char welcome[80];
	int welcomelen;
	int padding;

	ab_b = 0;
	ab_len = 0;

	ab_append("\x1b[?25l", 6);
	ab_append("\x1b[H", 3);

	for (y = 0; y < e_screenrows; y++) {
		filerow = e_rowoff + y;

		if (filerow >= e_numrows) {
			if (e_numrows == 0 && y == e_screenrows / 3) {
				welcomelen = snprintf(welcome, 80, "Milo Editor v3");
				if (welcomelen > e_screencols) {
					welcomelen = e_screencols;
				}

				padding = (e_screencols - welcomelen) / 2;
				if (padding > 0) {
					ab_append("~", 1);
					padding--;
				}
				while (padding-- > 0) {
					ab_append(" ", 1);
				}
				ab_append(welcome, welcomelen);
			} else {
				ab_append("~", 1);
			}
		} else {
			len = e_row_size[filerow] - e_coloff;
			if (len < 0) {
				len = 0;
			}
			if (len > e_screencols) {
				len = e_screencols;
			}

			if (len > 0) {
				ab_append(e_row_chars[filerow] + e_coloff, len);
			}
		}

		ab_append("\x1b[K", 3);
		if (y < e_screenrows - 1) {
			ab_append("\r\n", 2);
		}
	}

	snprintf(buf, 32, "\x1b[%d;%dH", (e_cy - e_rowoff) + 1, (e_cx - e_coloff) + 1);
	ab_append(buf, strlen(buf));

	ab_append("\x1b[?25h", 6);

	write(1, ab_b, ab_len);
	free(ab_b);
}

void init_editor()
{
	int rows;
	int cols;

	e_cx = 0;
	e_cy = 0;
	e_rowoff = 0;
	e_coloff = 0;
	e_numrows = 0;
	e_row_size = 0;
	e_row_chars = 0;
	e_dirty = 0;
	e_filename = 0;

	if (get_window_size(&rows, &cols) == -1) {
		exit(1);
	}
	e_screenrows = rows;
	e_screencols = cols;
}

int main(int argc, char **argv)
{
	int fd;

	enable_raw_mode();
	init_editor();

	if (argc >= 2) {
		e_filename = argv[1];
		fd = open(argv[1], MY_O_RDWR);
		if (fd != -1) {
			editor_append_row("Hello world!", 18);
		}
	}

	while (1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
