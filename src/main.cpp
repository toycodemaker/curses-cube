#include <ncurses.h>

char buff[512];

int main()
{	
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, true);

    char ch;
    while ((ch=getch())!='q') {
        printw("the key now is %c, the value is %d\n", ch, (int)ch);
        refresh();
    }

    endwin();
	return 0;
}
