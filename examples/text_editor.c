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
extern void *memmove(void *dest, const void *src, unsigned long n);
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
int *e_row_size;
char **e_row_chars;

/* Status */
int e_dirty;
char *e_filename;
char e_statusmsg[80];

/* Constants */
const int KEY_CTRL_Q = 17;
const int KEY_CTRL_S = 19;
const int KEY_ARROW_LEFT = 1000;
const int KEY_ARROW_RIGHT = 1001;
const int KEY_ARROW_UP = 1002;
const int KEY_ARROW_DOWN = 1003;
const int KEY_HOME = 1004;
const int KEY_END = 1005;
const int KEY_PAGE_UP = 1006;
const int KEY_PAGE_DOWN = 1007;
const int KEY_DEL = 1008;
const int KEY_BACKSPACE = 127;
const int MY_O_RDWR = 2;
const int MY_O_CREAT = 64;
const int MY_O_TRUNC = 512;

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
	e_dirty = 1;
}

void editor_row_insert_char(int at, int c)
{
	int row_idx;
	char *new_str;

	row_idx = e_cy;
	if (row_idx < 0 || row_idx >= e_numrows) {
		return;
	}

	if (at < 0 || at > e_row_size[row_idx]) {
		at = e_row_size[row_idx];
	}

	new_str = malloc(e_row_size[row_idx] + 2);
	memcpy(new_str, e_row_chars[row_idx], at);
	new_str[at] = c;
	memcpy(new_str + at + 1, e_row_chars[row_idx] + at, e_row_size[row_idx] - at);
	new_str[e_row_size[row_idx] + 1] = '\0';

	free(e_row_chars[row_idx]);
	e_row_chars[row_idx] = new_str;
	e_row_size[row_idx]++;
	e_dirty = 1;
}

void editor_row_del_char(int at)
{
	int row_idx;

	row_idx = e_cy;
	if (row_idx < 0 || row_idx >= e_numrows) {
		return;
	}

	if (at < 0 || at >= e_row_size[row_idx]) {
		return;
	}

	memmove(e_row_chars[row_idx] + at, e_row_chars[row_idx] + at + 1, e_row_size[row_idx] - at);
	e_row_size[row_idx]--;
	e_dirty = 1;
}

void editor_insert_char(int c)
{
	if (e_cy == e_numrows) {
		editor_append_row("", 0);
	}
	editor_row_insert_char(e_cx, c);
	e_cx++;
}

void editor_insert_newline()
{
	int new_size_int;
	int new_size_ptr;
	int i;
	char *new_row;
	int row_len;

	if (e_cy >= e_numrows) {
		editor_append_row("", 0);
		e_cy++;
		e_cx = 0;
		return;
	}

	new_size_int = (e_numrows + 1) * 4;
	new_size_ptr = (e_numrows + 1) * 8;

	e_row_size = realloc(e_row_size, new_size_int);
	e_row_chars = realloc(e_row_chars, new_size_ptr);

	for (i = e_numrows; i > e_cy + 1; i--) {
		e_row_size[i] = e_row_size[i - 1];
		e_row_chars[i] = e_row_chars[i - 1];
	}

	row_len = e_row_size[e_cy] - e_cx;
	if (row_len < 0) {
		row_len = 0;
	}

	new_row = malloc(row_len + 1);
	if (row_len > 0) {
		memcpy(new_row, e_row_chars[e_cy] + e_cx, row_len);
	}
	new_row[row_len] = '\0';

	e_row_size[e_cy + 1] = row_len;
	e_row_chars[e_cy + 1] = new_row;

	e_row_size[e_cy] = e_cx;
	e_row_chars[e_cy][e_cx] = '\0';

	e_numrows++;
	e_cy++;
	e_cx = 0;
	e_dirty = 1;
}

void editor_del_char()
{
	int prev_len;
	char *combined;

	if (e_cy >= e_numrows) {
		return;
	}

	if (e_cx == 0 && e_cy == 0) {
		return;
	}

	if (e_cx > 0) {
		editor_row_del_char(e_cx - 1);
		e_cx--;
	} else {
		int i;

		e_cx = e_row_size[e_cy - 1];
		prev_len = e_row_size[e_cy - 1];

		combined = malloc(prev_len + e_row_size[e_cy] + 1);
		memcpy(combined, e_row_chars[e_cy - 1], prev_len);
		memcpy(combined + prev_len, e_row_chars[e_cy], e_row_size[e_cy]);
		combined[prev_len + e_row_size[e_cy]] = '\0';

		free(e_row_chars[e_cy - 1]);
		e_row_chars[e_cy - 1] = combined;
		e_row_size[e_cy - 1] = prev_len + e_row_size[e_cy];

		free(e_row_chars[e_cy]);

		for (i = e_cy; i < e_numrows - 1; i++) {
			e_row_size[i] = e_row_size[i + 1];
			e_row_chars[i] = e_row_chars[i + 1];
		}

		e_numrows--;
		e_cy--;
		e_dirty = 1;
	}
}

void editor_open(char *filename)
{
	int fd;
	char buf[1024];
	int nread;
	int i;
	int line_start;

	fd = open(filename, MY_O_RDWR);
	if (fd == -1) {
		return;
	}

	line_start = 0;
	while (1) {
		nread = read(fd, buf, 1024);
		if (nread <= 0) {
			break;
		}

		for (i = 0; i < nread; i++) {
			if (buf[i] == '\n') {
				editor_append_row(buf + line_start, i - line_start);
				line_start = i + 1;
			}
		}

		if (line_start < nread) {
			editor_append_row(buf + line_start, nread - line_start);
			line_start = 0;
		} else {
			line_start = 0;
		}
	}

	close(fd);
	e_dirty = 0;
}

void editor_save()
{
	int fd;
	int i;
	int len;

	if (e_filename == 0) {
		snprintf(e_statusmsg, 80, "No filename");
		return;
	}

	fd = open(e_filename, MY_O_RDWR | MY_O_CREAT | MY_O_TRUNC);
	if (fd == -1) {
		snprintf(e_statusmsg, 80, "Can't save! Error opening file");
		return;
	}

	len = 0;
	for (i = 0; i < e_numrows; i++) {
		write(fd, e_row_chars[i], e_row_size[i]);
		write(fd, "\n", 1);
		len = len + e_row_size[i] + 1;
	}

	close(fd);
	e_dirty = 0;
	snprintf(e_statusmsg, 80, "%d bytes written", len);
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

void editor_scroll()
{
	if (e_cy < e_rowoff) {
		e_rowoff = e_cy;
	}
	if (e_cy >= e_rowoff + e_screenrows) {
		e_rowoff = e_cy - e_screenrows + 1;
	}
	if (e_cx < e_coloff) {
		e_coloff = e_cx;
	}
	if (e_cx >= e_coloff + e_screencols) {
		e_coloff = e_cx - e_screencols + 1;
	}
}

void editor_move_cursor(int key)
{
	int rowlen;

	rowlen = 0;
	if (e_cy < e_numrows) {
		rowlen = e_row_size[e_cy];
	}

	if (key == KEY_ARROW_LEFT) {
		if (e_cx != 0) {
			e_cx--;
		} else if (e_cy > 0) {
			e_cy--;
			e_cx = e_row_size[e_cy];
		}
	} else if (key == KEY_ARROW_RIGHT) {
		if (e_cx < rowlen) {
			e_cx++;
		} else if (e_cy < e_numrows) {
			e_cy++;
			e_cx = 0;
		}
	} else if (key == KEY_ARROW_UP) {
		if (e_cy != 0) {
			e_cy--;
		}
	} else if (key == KEY_ARROW_DOWN) {
		if (e_cy < e_numrows) {
			e_cy++;
		}
	} else if (key == KEY_HOME) {
		e_cx = 0;
	} else if (key == KEY_END) {
		if (e_cy < e_numrows) {
			e_cx = e_row_size[e_cy];
		}
	} else if (key == KEY_PAGE_UP) {
		e_cy = e_rowoff;
	} else if (key == KEY_PAGE_DOWN) {
		e_cy = e_rowoff + e_screenrows - 1;
		if (e_cy > e_numrows) {
			e_cy = e_numrows;
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

	if (c == KEY_CTRL_S) {
		editor_save();
		return;
	}

	if (c == 13) {
		editor_insert_newline();
		return;
	}

	if (c == KEY_BACKSPACE || c == 127) {
		editor_del_char();
		return;
	}

	if (c == KEY_DEL) {
		editor_move_cursor(KEY_ARROW_RIGHT);
		editor_del_char();
		return;
	}

	if (c == KEY_ARROW_UP || c == KEY_ARROW_DOWN || c == KEY_ARROW_LEFT || c == KEY_ARROW_RIGHT || c == KEY_HOME ||
	    c == KEY_END || c == KEY_PAGE_UP || c == KEY_PAGE_DOWN) {
		editor_move_cursor(c);
		return;
	}

	if (c >= 32 && c < 127) {
		editor_insert_char(c);
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

void editor_draw_status_bar()
{
	char status[80];
	char rstatus[80];
	int len;
	int rlen;

	ab_append("\x1b[7m", 4);

	len = snprintf(status, 80, "%.20s - %d lines %s", e_filename ? e_filename : "[No Name]", e_numrows,
		       e_dirty ? "(modified)" : "");

	rlen = snprintf(rstatus, 80, "%d/%d", e_cy + 1, e_numrows);

	if (len > e_screencols) {
		len = e_screencols;
	}
	ab_append(status, len);

	while (len < e_screencols) {
		if (e_screencols - len == rlen) {
			ab_append(rstatus, rlen);
			break;
		}
		ab_append(" ", 1);
		len++;
	}
	ab_append("\x1b[m", 3);
	ab_append("\r\n", 2);
}

void editor_draw_message_bar()
{
	int msglen;

	ab_append("\x1b[K", 3);
	msglen = strlen(e_statusmsg);
	if (msglen > e_screencols) {
		msglen = e_screencols;
	}
	if (msglen > 0) {
		ab_append(e_statusmsg, msglen);
	}
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

	editor_scroll();

	ab_b = 0;
	ab_len = 0;

	ab_append("\x1b[?25l", 6);
	ab_append("\x1b[H", 3);

	for (y = 0; y < e_screenrows - 2; y++) {
		filerow = e_rowoff + y;

		if (filerow >= e_numrows) {
			if (e_numrows == 0 && y == (e_screenrows - 2) / 3) {
				welcomelen = snprintf(welcome, 80, "Milo Editor v4 -- Ctrl-Q to quit, Ctrl-S to save");
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
		ab_append("\r\n", 2);
	}

	editor_draw_status_bar();
	editor_draw_message_bar();

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
	e_statusmsg[0] = '\0';

	if (get_window_size(&rows, &cols) == -1) {
		exit(1);
	}
	e_screenrows = rows;
	e_screencols = cols;
}

int main(int argc, char **argv)
{
	enable_raw_mode();
	init_editor();

	if (argc >= 2) {
		e_filename = argv[1];
		editor_open(argv[1]);
	}

	snprintf(e_statusmsg, 80, "HELP: Ctrl-S = save | Ctrl-Q = quit");

	while (1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
