#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "styles.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

// Variables to keep information of the screen
struct termios initial;
uint16_t width, height;
// Variables to keep track of the mapped file
char *p;
size_t bSize;
// FD of the random device
int randFd = -1;
// Array with index to the words in the mapped file
char **words;
uint32_t nWords = 0;
uint32_t sWords = 0;

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
    // In case we exit the program we need to restore everything
    atexit(restore);
    signal(SIGTERM, restore_die);
    signal(SIGINT, restore_die);
    say (
        esca alt_buf high
        esca term_clear
        esca curs low
    );
}

// We read a random value from /dev/urandom
uint32_t urandom() {
    uint32_t buf;
    read(randFd, &buf, sizeof(uint32_t));
    return buf;
}

void populateWords()
{
    uint32_t i;
    // Yep we manage our own memory
    words = malloc(1024*sizeof(char*));
    sWords = 1024;
    // The file always starts with the first word
    words[0]=&p[0];
    nWords = 1;
    // Loop through the file looking for the individual word and index them
    for (i=0; i<bSize-1; i++) {
        if (p[i] == '\n') {
            if (nWords >= sWords-1) {
                sWords = sWords+1024;
                // Yes we manage our own memory
                words = realloc(words, sWords*sizeof(char*));
            }
            words[nWords++] = &p[i+1];
        }
    }
}

int loadFile()
{
    int fp;
    struct stat statbuf;
    int status;

    fp = open("words.txt", O_RDONLY);
    if (!fp) return 1;

    status = fstat(fp, &statbuf);
    if (status!=0) return 2;
    // We map the file into memory so we can access as an array
    bSize = statbuf.st_size;
    p = mmap(NULL, bSize, PROT_READ, MAP_PRIVATE, fp, 0);
    if(p == MAP_FAILED) return 3;
    close(fp);

    randFd = open("/dev/urandom", O_RDONLY);
    if (randFd < 0) return 4;

    populateWords();

    return 0;
}

// Simple string size algorithm
int strSz(char *s)
{
    int i;
    for (i=0; s[i]!='\n'; i++);
    return i;
}

int getLine(char **line)
{
    uint32_t rand;
    uint32_t index;
    int size = 0;
    int aux;
    int i;
    char *s;

    s = malloc(128*sizeof(char));
    // Get a random word until we have a line that fits into a 70 char line
    while (1) {
        rand = urandom();
        index = rand % nWords;
        aux = strSz(words[index]);
        if (size+aux > 70) break;
        for (i=0; i<aux; i++) s[size++] = words[index][i];
        s[size++]=' ';
    }
    s[size]=0;
    (* line) = s;
    return size;
}

void paint(char key)
{
    static int first = 1;
    static int index = 0;
    static int line1Size=0;
    static char *line1;
    static int line2Size=0;
    static char *line2;
    int i;
    uint16_t mx = width/2;
    uint16_t my = height/2;

    if (first) {
        line1Size = getLine(&line1);
        printf(esca "%u" with "%u" jump fmt(plain) "%s",
                         my-2, mx-(line1Size/2), line1);
        line2Size = getLine(&line2);
        printf(esca "%u" with "%u" jump fmt(plain) "%s",
                           my, mx-(line2Size/2), line2);
        first=0;
        return;
    }
    if (key == line1[index]) index++;

    if (index >= line1Size) {
        // Free what we do not use anymore
        free(line1);
        line1=line2;
        line1Size=line2Size;
        index=0;

        line2Size = getLine(&line2);
        printf(esca "%u" with "%u" jump, my-2, 0);
        say(esca clear_line);
        printf(esca "%u" with "%u" jump, my, 0);
        say(esca clear_line);
        printf(esca "%u" with "%u" jump fmt(plain) "%s", my, mx-(line2Size/2), line2);
    }

    printf(esca "%u" with "%u" jump fmt(plain) "%c", my+2, mx, key);
    printf(esca "%u" with "%u" jump, my-2, mx-(line1Size/2));
    printf(fmt(underline));
    for(i=0; i<index; i++) printf("%c",line1[i]);
    printf(fmt(no underline));

    printf("%s", &line1[index]);
}

int main(int argc, char **argv)
{
	char inkey;
    int error = 0;
    error = loadFile();
    if (error != 0) return 1;

    initterm();
    signal(SIGWINCH, resize);
    resize(0);

    paint(' ');
    for (inkey; inkey != '\x1b';) {
        read(1,&inkey,1);
        paint(inkey);
    }

    return 0;
}
