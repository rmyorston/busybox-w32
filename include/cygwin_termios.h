/* iflag bits */
#define IGNBRK	0x00001
#define BRKINT	0x00002
#define IGNPAR	0x00004
#define IMAXBEL	0x00008
#define INPCK	0x00010
#define ISTRIP	0x00020
#define INLCR	0x00040
#define IGNCR	0x00080
#define ICRNL	0x00100
#define IXON	0x00400
#define IXOFF	0x01000
#define IUCLC	0x04000
#define IXANY	0x08000
#define PARMRK	0x10000

/* oflag bits */

#define OPOST	0x00001
#define OLCUC	0x00002
#define OCRNL	0x00004
#define ONLCR	0x00008
#define ONOCR	0x00010
#define ONLRET	0x00020
#define OFILL	0x00040
#define CRDLY	0x00180
#define CR0	0x00000
#define CR1	0x00080
#define CR2	0x00100
#define CR3	0x00180
#define NLDLY	0x00200
#define NL0	0x00000
#define NL1	0x00200
#define BSDLY	0x00400
#define BS0	0x00000
#define BS1	0x00400
#define TABDLY	0x01800
#define TAB0	0x00000
#define TAB1	0x00800
#define TAB2	0x01000
#define TAB3	0x01800
#define XTABS	0x01800
#define VTDLY	0x02000
#define VT0	0x00000
#define VT1	0x02000
#define FFDLY	0x04000
#define FF0	0x00000
#define FF1	0x04000
#define OFDEL	0x08000

/* lflag bits */
#define ISIG	0x0001
#define ICANON	0x0002
#define ECHO	0x0004
#define ECHOE	0x0008
#define ECHOK	0x0010
#define ECHONL	0x0020
#define NOFLSH	0x0040
#define TOSTOP	0x0080
#define IEXTEN	0x0100
#define FLUSHO	0x0200
#define ECHOKE	0x0400
#define ECHOCTL	0x0800

#define VDISCARD	1
#define VEOL		2
#define VEOL2		3
#define VEOF		4
#define VERASE		5
#define VINTR		6
#define VKILL		7
#define VLNEXT		8
#define VMIN		9
#define VQUIT		10
#define VREPRINT	11
#define VSTART		12
#define VSTOP		13
#define VSUSP		14
#define VSWTC		15
#define VTIME		16
#define VWERASE	17

#define TCSAFLUSH	1
#define TCSANOW		2
#define TCSADRAIN	3
#define TCSADFLUSH	4

typedef unsigned char cc_t;
typedef unsigned int  tcflag_t;
typedef unsigned int  speed_t;
typedef unsigned short otcflag_t;
typedef unsigned char ospeed_t;

#define NCCS		18
struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	char		c_line;
	cc_t		c_cc[NCCS];
	speed_t		c_ispeed;
	speed_t		c_ospeed;
};

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int mode,  const struct termios *t);
int is_cygwin_tty(int fd);
int wincon_read(int fd, char *buf, int size);
