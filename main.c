#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "styles.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

// Important code extracted from http://ʞ.cc/irl/term.html
// The words from here https://github.com/dwyl/english-words/blob/master/words_alpha.txt

struct termios initial;
uint16_t width, height;
char *p;
size_t bSize;
int randFd = -1;

void restore(void) {
	say(
		//enter alternate buffer if we haven't already
			esca alt_buf high
		//clean up the buffer
			esca term_clear
		//show the cursor
			esca curs high
		//return to the main buffer
			esca alt_buf low
	);
	//restore original termios params
	tcsetattr(1, TCSANOW, &initial);

}

void restore_die(int i) {
	exit(1);
}

void resize(int i) {
	struct winsize ws;
	ioctl(1, TIOCGWINSZ, &ws);
	width = ws.ws_col;
	height = ws.ws_row;
	say(esca term_clear);
}

void initterm(void) {
	setvbuf(stdout, NULL, _IONBF, 0);

	termios: {
		struct termios t;
		tcgetattr(1, &t);
		initial = t;
		t.c_lflag &= (~ECHO & ~ICANON);
		tcsetattr(1, TCSANOW, &t);
	};

	atexit(restore);
	signal(SIGTERM, restore_die);
	signal(SIGINT, restore_die);

	say (
		esca alt_buf high
		esca term_clear
		esca curs low
	);
}

uint32_t urandom() {
    uint32_t buf

    read(randFd, &buf, sizeof(uint32_t));
    return buf_impl;
}

int loadFile()
{
    int fp;
    struct stat statbuf;

    fp = open("words.txt", O_RDONLY);
    if (!fp) return 1;

    if (!fstat(fp, &statbuf)) return 1;

    bSize = statbuf.st_size;
    p = mmap(NULL, bSize, PROT_READ, MAP_PRIVATE, fp, 0);
    if(p == MAP_FAILED) return 1;

    close(fp);

    randFd = open("/dev/urandom", O_RDONLY);
    if (randFd < 0) return 1;

    return 0;
}

void paint(char key)
{
    uint16_t mx = width/2;
    uint16_t my = height/2;
    printf(esca "%u" with "%u" jump fmt(plain) "%c", my, mx, key);
}

int main(int argc, char **argv)
{
    printf("Loading file...\n");
    if (!loadFile()) return 1;
    initterm();
    signal(SIGWINCH, resize);
    resize(0);
    for (char inkey; inkey != '\x1b';) {
        read(1,&inkey,1);
        paint(inkey);
    }

    return 0;
}
