/*	File: os1shell.h
	Author: Liam Morris
	Description: Defines the functions that are used within the shell implementation.
*/
#ifndef OS1SHELL_H
#define OS1SHELL_H

// Main workhorse of the shell
// Accepts a command from stdin, decides how to process it, then executes it
int main(int argc, char** argv);

// These two functions clear a buffer by setting all entries to NULL
// &theBuffer - the array/buffer that is being cleared/flushed
void clearBuffer(char* &theBuffer);
void clearBuffer(char** &theBuffer);

// Gets a command from STDIN and stores it into a buffer
// &buff - the buffer that the command gets stored in
void getCommand(char* &buff);

// Handles all signals received (from 0-32), see os1shell.cpp to see how handled
// signal - the signal's number that is received
void signalHandler(int signal);
#endif
