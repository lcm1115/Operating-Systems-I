/*	File: os1shell.cpp
	Author: Liam Morris
	Description: Implements a basic shell program that to interface with
		     UNIX and run UNIX commands.
*/

#include "os1shell.h"
#include "history.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <cstdlib>
#include <termios.h>

using namespace std;
history* h;

// Various constants that are used throughout the program
// I/O constants
const int STDIN = 0;
const int STDERR = 1;
const int STDOUT = 2;
const int PROMPT_LENGTH = 10;

// Signal constants
const int SEG_FAULT = 10;
const int NUM_SIGNALS = 32;

// Max buffer size
const int MAX_BUFFER = 64;

// ASCII codes for various keys.. used when getting input
const int eof = 4;
const int ESCAPE = 27;
const int SPECIAL_KEY = 91;
const int UP_KEY = 65;
const int DOWN_KEY = 66;
const int BACKSPACE = 127;

// Determines if program should wait for a terminating process when it receives
// terminating signal (only should if it is running in background)
bool justWaitedForChild;

// Screen states for termios
termios before, after;

int main(int argc, char** argv) {
	// initialize screen states for use with termios
	tcgetattr(STDIN_FILENO, &before);
	after = before;
	after.c_lflag *= (~ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &after);
	h = new history();
	// create new process to clear screen (I like this, makes it nicer)
	int id = fork();
	if (id == 0) {
		execvp("clear", NULL);
	} else {
		waitpid(id, NULL, NULL);
	}
	// add all signals to signal handler
	for (int i = 0; i < NUM_SIGNALS; i++) {
		signal(i, signalHandler);
	}
	// initialize command buffers
	char* buff = new char[MAX_BUFFER];
	char** cmd = new char*[MAX_BUFFER];
	while(1) {
		// reset some variables, clear arg buffer
		bool waitForChild = 1;
		clearBuffer(cmd);

		// get a command and add to history
		getCommand(buff);
		h->add(buff);

		// was the command history, if so, print history
		// I did it this way so that if history & or something weird was called it just pritned history
		string s(buff);
		if(s.length() >= 7 && 
			strcmp(s.substr(0, 7).c_str(), "history") == 0) {
			h->print();
			continue;
		}

		// is the process running in the background?
		if (buff[strlen(buff)-1] == '&') {
			waitForChild = 0;
			buff[strlen(buff)-1] = 0;
		}

		// get argument tokens and store them in another buffer
		int count = 0;
		char* temp = strtok(buff, " ");
		while (temp != NULL) {
			cmd[count] = temp;
			count++;
			temp = strtok(NULL, " ");
		}

		// reset this variable so that we don't wait twice for one process
		justWaitedForChild = 0;

		// execute the command
		if (strcmp(cmd[0], "exit") == 0) {
			tcsetattr(STDIN_FILENO, TCSANOW, &before);
			exit(0);
		} else if (strcmp(cmd[0], "cd") != 0) {
			int pid = fork();

			// execute command on child process
			if (pid == 0) {
				execvp(cmd[0], cmd);
				exit(-1);
			}
			// wait for process to terminate (if running in foreground)
			else if (waitForChild) {
				justWaitedForChild = 1;
				waitpid(0, NULL, NULL);
			}
		} 
		// if "cd" was entered, change directory
		else {
			chdir(cmd[1]);
		}
	}
}

void signalHandler(int signal) {
	switch(signal) {
	// Received CTRL+C
	case SIGINT:
		// Display history
		h->add((char *) "history");
		cout << endl;
		h->print();
		write(STDIN, "os1shell> ", PROMPT_LENGTH);
		break;
	// Received CTRL+'\'
	case SIGQUIT:
		cout << endl << "Quit" << endl;
		// Reset screen then exit
		tcsetattr(STDIN_FILENO, TCSANOW, &before);
		delete(h);
		exit(0);
		break;
	// Child process is terminating
	case SIGCHLD:
		cout << "Child process terminated." << endl;
		if (!justWaitedForChild) waitpid(0, NULL, NULL);
		break;
	// Seg fault -- I left this in just in case.. (loops infinitely
	// if this is not here and a seg fault occurs, although seg faults
	// don't happen anymore to my knowledge)
	case SEG_FAULT:
		cout << "Segmentation fault." << endl;
		// Reset screen then exit
		tcsetattr(STDIN_FILENO, TCSANOW, &before);
		delete(h);
		exit(0);
	// Some other signal -- print what we received
	default:
		fprintf(stderr, "Intercepted signal %d\n", signal);
	}
}

void clearBuffer(char* &theBuffer) {
	for (int i = 0; i < MAX_BUFFER; i++) {
		theBuffer[i] = 0;
	}
}

void clearBuffer(char** &theBuffer) {
	for (int i = 0; i < MAX_BUFFER; i++) {
		theBuffer[i] = 0;
	}
}

void resetLine(char* &buff) {
	// Move to beginning of line
	cout << '\r';
	// Print spaces to overwrite existing characters on line
	for (int j = 0; j < MAX_BUFFER + PROMPT_LENGTH; j++) {
		cout << ' ';
	}
	// Move to beginning of line again and print prompt and whateve ris in buffer
	cout << '\r';
	cout << "os1shell> " << buff;
}

void getCommand(char* &buff) {
	// clear command buffer and print prompt
	clearBuffer(buff);
	cout << "os1shell> ";
	char ch;
	int i = 0;
	// current command we're looking at in history
	node* curCommand = 0;
	while(true) {
		if (i > MAX_BUFFER) {
			cout << endl << "ERROR: Command too long" << endl;
			getCommand(buff);
			break;
		}
		// get character and start figuring out what to do
		cin.get(ch);
		if ((int) ch == SPECIAL_KEY) {
			cin.get(ch);
			// if up arrow key and history exists, begin
			// traversing history upwards
			if ((int) ch == UP_KEY && h->getCount() > 0) {
				// if not looking at command yet, get most
				// recently entered
				if (curCommand == 0) {
					curCommand = h->head;
				}
				// otherwise move to next most recent command
				// until last command in history is highlighted,
				// then highlight head again
				else {
					if (curCommand->next != 0) {
						curCommand = curCommand->next;
					}
					else {
						curCommand = h->head;
					}
				}
				// copy highlighted command into buffer
				strcpy(buff, curCommand->command);
			} 
			// if down key and history exists, begin traversing down
			else if ((int) ch == DOWN_KEY && h->getCount() > 0) {
				// these are the same as for up key, but
				// in reverse
				if (curCommand == 0) {
					curCommand = h->tail;
				} else {
					if (curCommand->prev != 0) {
						curCommand = curCommand->prev;
					} else {
						curCommand = h->tail;
					}
				}
				// copy highlighted command into buffer
				strcpy(buff, curCommand->command);
			}
			// reset command line (to display new command)
			resetLine(buff);
			i = strlen(buff);
		// if backspace entered	
		} else if ((int) ch == 127) {
			// erase last index
			if (i > 0) {
				i--;
				buff[i] = 0;
			}
			// reset command line (backspace shows up otherwise
			// due to echoing of special characters [to detect
			// the arrow keys] )
			resetLine(buff);
		// if CTRL + D is entered, exit
		} else if ((int) ch == eof) {
			resetLine(buff);
			cout << endl << "Terminating" << endl;
			tcsetattr(STDIN_FILENO, TCSANOW, &before);
			delete(h);
			exit(0);
		// if enter is hit, exit loop (unless command is empty)
		} else if (ch == '\n') {
			if (i == 0) {
				cerr << "No command entered" << endl;
				getCommand(buff);
			}
			break;
		// ignore escape key (arrow keys enter this)
		} else if (ch == ESCAPE) {
			continue;
		// normal char? add it to buffer
		} else {
			if (i < MAX_BUFFER) buff[i] = ch;
			i++;
		}
	}
}
