#ifndef TERMIOS_H
#define TERMIOS_H

#define VINTR 0
#define VEOF  1

#define TCIFLUSH    0
#define TCSAFLUSH   1
#define TCSANOW     2
#define TCSADRAIN   3
#define TCSADFLUSH  4

typedef unsigned char cc_t;
typedef unsigned int  speed_t;

#define NCCS		2
struct termios {
	cc_t		c_cc[NCCS];
	unsigned long	imode;
	unsigned long	omode;
};

struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int mode, const struct termios *t);

#endif /* TERMIOS_H */
