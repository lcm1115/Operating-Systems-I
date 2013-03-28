/*	File: os1shell.h
	Author: Liam Morris
	Description: Defines the functions that are used within the shell implementation.
*/
#ifndef OS1SHELL_H
#define OS1SHELL_H
#include <vector>
typedef struct {
	char name[112];
	unsigned int index;
	unsigned int size;
	unsigned int type;
	unsigned int creation;
} directory_entry;

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

void handleCommand(char** cmd);
void printDT();
void printFAT();

int fileIndex(char* const &fileName);
int fileEntry(char* const &fileName);

std::vector<char> readCluster(int clusterIndex);
int numAvailableClusters();
std::vector<char> readFile(int clusterIndex, int size);
void writeFile(char* file_name, std::vector<char> data);
void removeFile(char* const &file_name);

void readCluster(int clusterIndex, char* &data);
void writeCluster(int clusterIndex, char* &data, unsigned int size);

int findAvailableEntry();
int writeFATRecord(int writeIndex);
int findAvailableCluster();

void updateDT();
void updateFAT();
void writeFAT();
void listContents();
#endif
