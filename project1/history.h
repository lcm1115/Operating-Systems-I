/*	File: history.h
	Author: Liam Morris
	Description: Blueprints the functions and data members of a basic 
		     linked list to be used for command history.
*/
#ifndef HISTORY_H
#define HISTORY_H

// Simple node class used to make up the history
class node {
public:
	// constructor
	// c - command to initialize the node with
	node(char* c);
	
	// destructor - has pointers that need to be cleaned up
	~node();

	// data members (names are self explanatory)
	char* command;
	node* next;
	node* prev;
};

// A class that is a linked list of nodes to keep track of commands that
// have been entered into a shell
class history {
public:
	history();
	void add(char* command);

	// prints out the history starting from oldest entry (most recently
	// entered shows up at the bottom)
	void print();
	int getCount();

	// need to clear out the pointers!
	~history();

	node* head;
	node* tail;

private:
	int count;
};
#endif
