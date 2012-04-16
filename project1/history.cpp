/*	File: history.cpp
	Author: Liam Morris
	Description: Implements the functions described in history.h to
		     provide a basic linked list to store a command history.
*/

#include <cstdlib>
#include <iostream>
#include "history.h"
#include <string.h>

// Initialize data -- count = 0, head and tail are empty
history::history() : count(0), head(NULL), tail(NULL) { }

history::~history() {
	// Start from head
	node* current = head;
	if (current != NULL) {
		// Iterate until the end, deleting nodes as we go
		while (current->next != NULL) {
			current = current->next;
			delete current->prev;
		}
		delete current;
	}
}

void history::add(char* command) {
	using namespace std;

	// Create a new node and store the command
	char* theCommand = new char[64];
	strcpy(theCommand, command);
	node* newNode = new node(theCommand);

	// For first entry, store it as the head and the tail
	if (count == 0) {
		head = newNode;
		tail = newNode;
		count++;
	}
	// If 20 haven't been entered yet, continue to add new nodes to
	// the front of the history
	else if (count < 20) {
		head->prev = newNode;
		newNode->next = head;
		head = newNode;
		count++;
	}
	// If 20 commands are in the history, get rid of the tail
	else {
		head->prev = newNode;
		newNode->next = head;
		head = newNode;
		node* temp = tail;
		tail = tail->prev;
		tail->next = NULL;
		delete temp;
	}
}

void history::print() {
	std::cout << "Command History" << std::endl;
	std::cout << "---------------" << std::endl;
	// Starting from the tail, iterate backwards and print the commands
	node* current = tail;
	while (current != NULL) {
		std::cout << current->command << std::endl;
		current = current->prev;
	}
}

int history::getCount() {
	return count;
}

// Initialize node with command, next and prev are null
node::node(char* c) : command(c), next(NULL), prev(NULL) { }

node::~node() {
	// Delete the char* contained within command
	delete command;
}
