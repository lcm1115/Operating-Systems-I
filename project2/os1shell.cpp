/*	File: os1shell.cpp
	Author: Liam Morris
	Description: Implements a basic shell program that to interface with
		     UNIX and run UNIX commands.
		     Project 2 Update: Now emulates a FAT16 file system and supports the commands
		     required by the project.
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
#include <fstream>
#include <iomanip>
#include <vector>

using namespace std;
history* h;

// Various constants that are used throughout the program
// I/O constants
const int STDIN = 0;
const int STDERR = 1;
const int STDOUT = 2;
const int PROMPT_LENGTH = 10;

// Max buffer size
const int MAX_BUFFER = 64;

// ASCII codes for various keys.. used when getting input
const int eof = 4;
const int ESCAPE = 27;
const int SPECIAL_KEY = 91;
const int UP_KEY = 65;
const int DOWN_KEY = 66;
const int BACKSPACE = 127;


// File system values
unsigned int fs_size;
unsigned int cluster_size;
unsigned int root_index;
unsigned int FAT_index;
int num_clusters;
char* fs_name;
char* fs_dir;
bool in_fs;

// Determines if program should wait for a terminating process when it receives
// terminating signal (only should if it is running in background)
bool justWaitedForChild;

unsigned int bootrecord[4];
unsigned int* FileAllocationTable;
string mount;

directory_entry* dir_table;

int main(int argc, char** argv) {
	// initialize screen states for use with termios
	h = new history();
	in_fs = false;
	// create new process to clear screen (I like this, makes it nicer)
	int id = fork();
	if (id == 0) {
		execvp("clear", NULL);
	} else {
		waitpid(id, NULL, NULL);
	}
	fs_dir = get_current_dir_name();
	if (argc > 1) {
		in_fs = true;
		fs_name = argv[1];
		FILE* fp = fopen(argv[1], "r+");
		mount = "/";
		mount = strcat(const_cast<char *>(mount.c_str()), fs_name);
		// Does FS exist?
		if (fp) {
			fseek(fp, 0, SEEK_SET);
			// Read in boot record values
			fread(&bootrecord, sizeof(unsigned int)*4, 1, fp);
			cluster_size = bootrecord[0];
			fs_size = bootrecord[1];
			root_index = bootrecord[2];
			FAT_index = bootrecord[3];
			fseek(fp, root_index * cluster_size, SEEK_SET);

			// Initialize directory table and FAT
			dir_table = (directory_entry *)calloc(cluster_size, sizeof(directory_entry));
			fread(dir_table, cluster_size, 1, fp);
			fseek(fp, FAT_index * cluster_size, SEEK_SET);
			FileAllocationTable = (unsigned int *)calloc(cluster_size, sizeof(unsigned int));
			fread(FileAllocationTable, cluster_size, 1, fp);
		} else {
			string in;

			// Construct file system based on inputs
			cout << "Are you sure you want to create a new filesystem [Y]? ";
			getline(cin, in);
			if (strcmp(in.c_str(), "y") != 0 and strcmp(in.c_str(), "Y") != 0) {
				cout << "Exiting." << endl;
				exit(0);
			}
			cout << "Enter the maximum size for this file system in MB: ";
			int size;
			getline(cin, in);
			size = atof(in.c_str());
			// Validate size
			while (size < 5 || size > 50) {
				cout << "Error: Invalid size, try again: ";
				getline(cin, in);
				size = atof(in.c_str());
			}
			fs_size = size * 1024 * 1024;
			bootrecord[1] = fs_size;

			cout << "Enter the cluster size for this file system in KB: ";
			getline(cin, in);
			size = atof(in.c_str());
			// Validate size
			while (size < 8 || size > 16) {
				cout << "Error: Invalid size, try again: ";
				getline(cin, in);
				size = atof(in.c_str());
			}
			cluster_size = size * 1024;
			if (fs_size / cluster_size > cluster_size) {
				cerr << "FAT table will not fit in one cluster. Exiting." << endl;
				exit(0);
			}

			// Initialize boot record, write file system to file
			bootrecord[0] = cluster_size;
			bootrecord[2] = 2;
			root_index = 2;
			bootrecord[3] = 1;
			FAT_index = 1;
			fp = fopen(fs_name, "a");
			cout << "Initializing file system. Please be patient if it is large! :)" << endl;
			unsigned int* fs = (unsigned int*)calloc(cluster_size, sizeof(unsigned int));
			for (int i = 0; i < fs_size / cluster_size; i++) {
				fwrite(fs, cluster_size, 1, fp);
			}
			free(fs);
			fclose(fp);
			fopen(fs_name, "r+");
			directory_entry init_entry;
			init_entry.name[0] = 0x00;
			init_entry.size = 0;
			init_entry.index = 0;
			init_entry.type = 0;
			init_entry.creation = time(0);
			fseek(fp, 0, SEEK_SET);
			fwrite(bootrecord, sizeof(int) * 4, 1, fp);
			// Initialize directory table and FAT
			dir_table = (directory_entry *)calloc(cluster_size, sizeof(directory_entry));
			FileAllocationTable = (unsigned int*)calloc(cluster_size, sizeof(unsigned int));
			FileAllocationTable[0] = 0xFFFF;
			FileAllocationTable[root_index] = 0xFFFF;
			FileAllocationTable[FAT_index] = 0xFFFF;
			fseek(fp, FAT_index * cluster_size, SEEK_SET);
			fwrite(FileAllocationTable, cluster_size, 1, fp);
		}
		fclose(fp);
		num_clusters = fs_size / cluster_size;
	}
	// initialize command buffers
	string buff;
	char** cmd = new char*[MAX_BUFFER];
	while(1) {
		// reset some variables, clear arg buffer
		bool waitForChild = 1;
		clearBuffer(cmd);
		cout << "os1shell> ";
		// get a command and add to history
		getline(cin, buff);
		if (buff.length() == 0) continue;
		h->add(const_cast<char *>(buff.c_str()));
		// was the command history, if so, print history
		// I did it this way so that if history & or something weird was called it just pritned history
		if(buff.length() >= 7 && 
			strcmp(buff.substr(0, 7).c_str(), "history") == 0) {
			h->print();
			continue;
		}

		// is the process running in the background?
		if (buff.at(buff.length()-1) == '&') {
			waitForChild = 0;
			buff.resize(buff.length()-1);
		}

		// get argument tokens and store them in another buffer
		int count = 0;
		char* temp = strtok(const_cast<char *>(buff.c_str()), " ");
		while (temp != NULL) {
			cmd[count] = temp;
			count++;
			temp = strtok(NULL, " ");
		}

		// reset this variable so that we don't wait twice for one process
		justWaitedForChild = 0;

		string dir;
		if (count > 1) {
			dir = cmd[1];
			dir = dir.substr(0, strlen(mount.c_str()));
		}
		
		// execute the command
		if (strcmp(cmd[0], "exit") == 0) {
			free(cmd);
			break;
		} else if (strcmp(cmd[0], "cd") != 0) {
			// Components of argument, used for determining if we are inside the file system or not
			string arg1;
			string arg1_fs;
			string arg2;
			string arg2_fs;

			// Initializes the variables to be used for comparisons
			if (count > 1) {
				arg1 = cmd[1];
				arg1_fs = cmd[1];
				if (arg1.find("/") != -1) arg1 = arg1.substr(0, strlen(mount.c_str()));
				if (count > 2) {
					arg2 = cmd[2];
					arg2_fs = cmd[2];
					if (arg2.find("/") != -1) arg2 = arg2.substr(0, strlen(mount.c_str()));
				}
			}

			// Determine if we need to process the command using the internal file system
			// Conditions for using the file system:
			// If 3 arguments exist (including command)
			//	Either parameter contains mount point
			//	Currently in file system and either of the parameter is local
			// If 2 arguments exist (including command)
			//	Currently in file system and parameter is local
			//	The parameter contains mount point
			bool args_in_fs = (count == 3 && 
						((strcmp(arg1.c_str(), mount.c_str()) == 0 || strcmp(arg2.c_str(), mount.c_str()) == 0)
						|| (in_fs && arg1_fs.find("/") == -1)
						|| (in_fs && arg2_fs.find("/") == -1)))
						|| (count == 2 && ((in_fs && (arg1_fs.find("/") == -1)) || strcmp(arg1.c_str(), mount.c_str()) == 0));
						
			// If we don't need to use the file system, process the command like in project 1
			if ((!in_fs && count == 1)
				|| (count > 1 && !args_in_fs)) {
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
			} else {
				handleCommand(cmd);
			}
		}
		// if "cd" was entered, change directory
		else {
			if (strcmp(mount.c_str(), dir.c_str()) == 0) {
				chdir(fs_dir);
				in_fs = true;
			} else {
				chdir(cmd[1]);
				in_fs = false;
			}
		}
	}
}

/* Handles commands that require interfacing with the internal file system.
 * char** cmd - the command that is being handled
 */
void handleCommand(char** cmd) {
	// If printDT, printFAT, ls, or df, handle the command appropriately
	if (strcmp(cmd[0], "printDT") == 0) printDT();
	else if (strcmp(cmd[0], "printFAT") == 0) printFAT();
	else if (strcmp(cmd[0], "ls") == 0) listContents();
	else if (strcmp(cmd[0], "df") == 0) {;
		char numBlocks[20];
		sprintf(numBlocks, "%d", (cluster_size / 1024));
		strcat(numBlocks, (char *) "K-Blocks");
		// Print out first line of df
		cout << setw(11) << "File System" << setw(15) << numBlocks
		     << setw(15) << "Used" << setw(15) << "Available" << setw(10)
		     << "Used%" << setw(15) << "Mount Point" << endl;
		int available_clusters = numAvailableClusters();
		// Print out second line
		cout << setw(11) << fs_name << setw(15) << (num_clusters) << setw(15) << (num_clusters - available_clusters)
		     << setw(15) << available_clusters << setw(10) << setprecision(4) << (((float) (num_clusters - available_clusters)) / num_clusters * 100)
		     << setw(15) << mount << endl;
	}
	// Create a 0 byte file
	else if (strcmp(cmd[0], "touch") == 0) {
		string temp(cmd[1]);
		if (temp.find(mount.c_str()) != -1) temp = temp.substr(strlen(mount.c_str()) + 1);
		char* file_name = (char *) temp.c_str();
		int write_index = fileIndex(file_name);
		// If the file exists, remove it
		if (write_index == -1) {
			write_index = findAvailableCluster();
		} else {
			removeFile(file_name);
		}

		// Update FAT and directory table accordingly
		FileAllocationTable[write_index] = 0xFFFF;
		int entry_index = fileEntry(file_name);
		if (entry_index == -1) entry_index = findAvailableEntry();
		directory_entry* entry = new directory_entry;
		strcpy(entry->name, file_name);
		entry->size = 0;
		entry->type = 0;
		entry->creation = time(0);
		entry->index = write_index;
		FILE* fp = fopen(fs_name, "r+");
		fseek(fp, entry_index, SEEK_SET);
		fwrite(entry, sizeof(directory_entry), 1, fp);
		fclose(fp);
		writeFAT();
	}
	// Print contents of a file
	else if (strcmp(cmd[0], "cat") == 0) {
		string temp(cmd[1]);
		if (temp.find(mount.c_str()) != -1) temp = temp.substr(strlen(mount.c_str()) + 1);
		char* file_name = (char *) temp.c_str();
		int entry_index = fileEntry(file_name);
		// Make sure it exists
		if (entry_index != -1) {
			// Get the entry and it's info
			directory_entry* entry = (directory_entry*)malloc(sizeof(directory_entry));
			char* cur_path = get_current_dir_name();
			chdir(fs_dir);
			FILE* fp = fopen(fs_name, "r");
			fseek(fp, entry_index, SEEK_SET);
			fread(entry, sizeof(directory_entry), 1, fp);
			fclose(fp);
			chdir(cur_path);
			free(cur_path);
			char* data = (char*)malloc(cluster_size);
			int cur_index = entry->index;
			// Iterate through file's FAT entries printing each cluster
			while (cur_index != 0xFFFF) {
				readCluster(cur_index, data);
				cout << data;
				cur_index = FileAllocationTable[cur_index];
			}
			cout << endl;
			free(data);
			free(entry);
		}
		else cerr << "File does not exist." << endl;
	}
	// Remove a file
	else if (strcmp(cmd[0], "rm") == 0) {
		string temp(cmd[1]);
		if (temp.find(mount.c_str()) != -1) temp = temp.substr(strlen(mount.c_str()) + 1);
		char* file_name = (char *) temp.c_str();
		int read_index = fileIndex(file_name);
		// If it exists, remove it
		if (read_index != -1) {
			removeFile(file_name);
		}
		else cerr << "File does not exist." << endl;
	}
	// Copying a file (or moving a file)
	else if (strcmp(cmd[0], "cp") == 0 || strcmp(cmd[0], "mv") == 0) {
		string src = cmd[1];
		string dest = cmd[2];
		bool copyFromFS;
		bool copyToFS;
		// Determine if copying from the file system
		if (src.find("/") != -1) {
			src = src.substr(0, strlen(mount.c_str()));
			copyFromFS = (strcmp(src.c_str(), mount.c_str()) == 0);
		}
		else {
			copyFromFS = in_fs;
		}
		
		// Determine if copying to the file system
		if (dest.find("/") != -1) {
			dest = dest.substr(0, strlen(mount.c_str()));
			copyToFS = (strcmp(dest.c_str(), mount.c_str()) == 0);
		}
		else {
			copyToFS = in_fs;
		}
		
		string source = cmd[1];
		string destination = cmd[2];
		char* data = (char*)malloc(cluster_size);
		// Calculate free space in the system
		unsigned int free_space = numAvailableClusters() * cluster_size;
		if (copyFromFS) {
			if (source.find("/") != -1) source = source.substr(strlen(mount.c_str()) + 1);
			char* file_name = (char*) source.c_str();
			int entry_index = fileEntry(file_name);
			// Make sure file exists
			if (entry_index != -1) {
				directory_entry* entry = new directory_entry;
				char* cur_path = get_current_dir_name();
				chdir(fs_dir);
				FILE* fp = fopen(fs_name, "r");
				fseek(fp, entry_index, SEEK_SET);
				fread(entry, sizeof(directory_entry), 1, fp);
				fclose(fp);
				chdir(cur_path);
				// Make sure the file is an actual file and that it will fit
				if (entry->type == 0x0000 && (!copyToFS || (copyToFS && free_space >= entry->size))) {
					int cur_index = entry->index;
					int write_index = findAvailableCluster();
					int leftToWrite = entry->size;
					int write_size = cluster_size;
					ofstream file_stream;
					// If copying to the file system, add an entry
					if (copyToFS) {
						directory_entry* new_entry = new directory_entry;
						if (destination.find("/") != -1) destination = destination.substr(strlen(mount.c_str()) + 1);
						strcpy(new_entry->name, destination.c_str());
						new_entry->size = entry->size;
						new_entry->creation = time(0);
						new_entry->index = cur_index;
						new_entry->type = entry->type;
						chdir(fs_dir);
						FILE* fp = fopen(fs_name, "r+");
						// Delete the file if it exists
						int entry_index = fileEntry(new_entry->name);
						if (entry_index != -1) removeFile(new_entry->name);
						else entry_index = findAvailableEntry();
						fseek(fp, entry_index, SEEK_SET);
						fwrite(new_entry, sizeof(directory_entry), 1, fp);
						fclose(fp);
						chdir(cur_path);
						free(new_entry);
					} else {
						// Delete the file if it exists
						remove(destination.c_str());
						file_stream.open(destination.c_str(), ios_base::app);
					}
					// Read a cluster then write that cluster of data until there is ntohing left to read
					while (cur_index != 0xFFFF && leftToWrite > 0) {
						if (leftToWrite < write_size) write_size = leftToWrite;
						readCluster(cur_index, data);
						if (copyToFS) {
							writeCluster(write_index, data, write_size);
							write_index = writeFATRecord(write_index);
						} else {
							file_stream.write(data, write_size);
						}
						cur_index = FileAllocationTable[cur_index];
						leftToWrite -= cluster_size;
					}
					if (!copyToFS) file_stream.close();
				}
				else if (copyToFS && free_space < entry->size) cerr << "Not enough free space in system." << endl;
				else cerr << "Cannot copy directory." << endl;
				free(cur_path);
				free(entry);
				if (strcmp(cmd[0], "mv") == 0) {
					removeFile(file_name);
				}
			}
			else {
				cerr << "Source file '" << file_name << "'does not exist." << endl;
			}
		}
		else {
			FILE* read_file = fopen(source.c_str(), "r");
			// Make sure the file exists
			if (read_file) {
				fseek(read_file, 0, SEEK_END);
				unsigned int leftToWrite = ftell(read_file);
				// Make sure the file will fit
				if (free_space >= leftToWrite) {
					unsigned int write_size = cluster_size;
					int write_index = findAvailableCluster();
					ofstream file_stream;
					directory_entry* new_entry = new directory_entry;
					if (destination.find("/") != -1) destination = destination.substr(strlen(mount.c_str()) + 1);
					strcpy(new_entry->name, destination.c_str());
					new_entry->size = leftToWrite;
					new_entry->creation = time(0);
					new_entry->index = write_index;
					new_entry->type = 0x0000;
					char* cur_path = get_current_dir_name();
					chdir(fs_dir);
					FILE* fp = fopen(fs_name, "r+");
					// Delete the file if it exists
					int entry_index = fileEntry(new_entry->name);
					if (entry_index != -1) removeFile(new_entry->name);
					else entry_index = findAvailableEntry();
					fseek(fp, entry_index, SEEK_SET);
					fwrite(new_entry, sizeof(directory_entry), 1, fp);
					fclose(fp);
					chdir(cur_path);
					free(new_entry);
					FileAllocationTable[write_index] = 0xFFFF;
					fseek(read_file, 0, SEEK_SET);
					// Read a cluster of data and write a cluster until the file is completely written
					while (leftToWrite > 0) {
						if (leftToWrite < write_size) write_size = leftToWrite;
						fread(data, write_size, 1, read_file);
						fseek(read_file, ftell(read_file) + cluster_size, SEEK_SET);
						writeCluster(write_index, data, write_size);
						leftToWrite -= write_size;
						if (leftToWrite > 0) write_index = writeFATRecord(write_index);
					}
					free(cur_path);
				}
				else cerr << "Not enough free space in system." << endl;
			}
			else {
				cerr << "Source file does not exist." << endl;
			}
			fclose(read_file);
		}
		free(data);
		writeFAT();
	}
	// Update the tables
	updateFAT();
	updateDT();
}

// Returns an int representing the number of clusters free to write to in the system
int numAvailableClusters() {
	int count = 0;
	for (int i = 0; i < num_clusters; i++) {
		if (FileAllocationTable[i] == 0x0000) {
			count++;
		}
	}
	return count;
}


// Clears a buffer of all characters (used for project 1)
void clearBuffer(char* &theBuffer) {
	for (int i = 0; i < MAX_BUFFER; i++) {
		theBuffer[i] = 0;
	}
}

// Clears a buffer of all cstrings (used for project 1)
void clearBuffer(char** &theBuffer) {
	for (int i = 0; i < MAX_BUFFER; i++) {
		theBuffer[i] = 0;
	}
}

/* Removes a file from the internal file system.
 * char* file_name - the name of the file to be removed
 */
void removeFile(char* const &file_name) {
	// Get the file's location in the file system
	int read_index = fileIndex(file_name);
	int entry_index = fileEntry(file_name);
	char* cur_path = get_current_dir_name();
	char* empty = (char *)calloc(sizeof(directory_entry), sizeof(char));
	chdir(fs_dir);
	FILE* fp = fopen(fs_name, "r+");
	fseek(fp, entry_index, SEEK_SET);
	fwrite(empty, sizeof(directory_entry), 1, fp);
	char* empty_cluster = (char*)calloc(cluster_size, sizeof(char));
	// If it is one cluster in length, write an empty cluster to that cluster
	if (FileAllocationTable[read_index] == 0xFFFF) {
		fseek(fp, read_index * cluster_size, SEEK_SET);
		fwrite(empty_cluster, cluster_size, 1, fp);
		FileAllocationTable[read_index] = 0x0000;
	} 
	// Otherwise iterate across all clusters and write an empty cluster to each
	else { 
		do {
			fseek(fp, read_index * cluster_size, SEEK_SET);
			fwrite(empty_cluster, cluster_size, 1, fp);
			unsigned int next_index = FileAllocationTable[read_index];
			FileAllocationTable[read_index] = 0x0000;
			read_index = next_index;
		} while (read_index != 0xFFFF);
	}
	fclose(fp);
	chdir(cur_path);
	free(cur_path);
	free(empty_cluster);
	free(empty);
	writeFAT();
}

/* Gets the starting FAT index of a file in the file system.
 * char* file_name - the name of the file
 */
int fileIndex(char* const &file_name) {
	int cur_index = root_index;
	// Initialize directory table and an entry
	directory_entry* table = (directory_entry*)malloc(cluster_size);
	directory_entry* cur_entry = (directory_entry*)malloc(sizeof(directory_entry));
	do {
		FILE* fp = fopen(fs_name, "r");
		fseek(fp, cur_index * cluster_size, SEEK_SET);
		fread(table, cluster_size, 1, fp);
		fclose(fp);
		// Loop across all entries in the table. If the file is found, return the index.
		for (int i = 0; i < cluster_size / 128; i++) {
			cur_entry = &table[i];
			if (strcmp(cur_entry->name, file_name) == 0) {
				return cur_entry->index;
			}
		}
		// Find the next directory table and use it
		cur_index = FileAllocationTable[cur_index];
	} while (cur_index != 0xFFFF);
	// Return -1 if not found
	return -1;
}

/* Finds the index of a file in the directory table.
 * char* file_name - the name of the file
 */
int fileEntry(char* const &file_name) {
	// Start from the root directory table
	int cur_index = root_index;
	directory_entry* table = (directory_entry*)malloc(cluster_size);
	directory_entry* cur_entry = (directory_entry*)malloc(128);
	do {
		FILE* fp = fopen(fs_name, "r");
		fseek(fp, cur_index * cluster_size, SEEK_SET);
		fread(table, cluster_size, 1, fp);
		fclose(fp);
		// Iterate across until the entry is found. If it is found, return its absolute index in the file system
		for (int i = 0; i < cluster_size / sizeof(directory_entry); i++) {
			cur_entry = &table[i];
			if (strcmp(cur_entry->name, file_name) == 0) {
				return i * sizeof(directory_entry) + cur_index * cluster_size;
			}
		}
		cur_index = FileAllocationTable[cur_index];
	} while (cur_index != 0xFFFF);
	// Return -1 if not found
	return -1;
}

/* Prints the directory tree in a readable format.
 */
void printDT() {
	directory_entry* cur_entry;
	int table_count = 0;
	int cur_index = root_index;
	updateDT();
	updateFAT();
	do {
		FILE* fp = fopen(fs_name, "r");
		fseek(fp, cluster_size * cur_index, SEEK_SET);
		directory_entry* new_table = (directory_entry *)malloc(cluster_size);
		fread(new_table, cluster_size, 1, fp);
		fclose(fp);
		// Iterate across each entry and print its information
		for (int i = 0; i < cluster_size / sizeof(directory_entry); i++) {
			cur_entry = &new_table[i];
			cout << "Entry " << i + (table_count * cluster_size / sizeof(directory_entry)) << endl;
			cout << "\tName: " << cur_entry->name << endl;
			cout << "\tIndex: " << cur_entry->index << endl;
			cout << "\tSize: " << cur_entry->size << endl;
			cout << "\tType: " << cur_entry->type << endl;
			cout << "\tCreation: " << cur_entry->creation << endl;
		}
		table_count++;
		// Get the next table (if it exists)
		cur_index = FileAllocationTable[cur_index];
	} while (cur_index != 0xFFFF);
}

/* Handles the 'ls' command and prints out files along with their information.
 */
void listContents() {
	// Initialize table and entry
	directory_entry* table = (directory_entry*)malloc(cluster_size);
	directory_entry* entry = (directory_entry*)malloc(sizeof(directory_entry));
	int cur_index = root_index;
	FILE* fp = fopen(fs_name, "r");
	do {
		char* cur_path = get_current_dir_name();
		chdir(fs_dir);
		fseek(fp, cur_index * cluster_size, SEEK_SET);
		fread(table, cluster_size, 1, fp);
		chdir(cur_path);
		// Iterate across each entry, print out information for the nonempty entries.
		for (int i = 0; i < cluster_size / 128; i++) {
			entry = &table[i];
			if (strcmp(entry->name, "") == 0) continue;
			cout << setw(15) << entry->size;
			cout << setw(20) << entry->name;
			cout << setw(10) << entry->type;
			time_t the_time(entry->creation);
			cout << setw(40) << asctime(localtime(&the_time));
		}
		cur_index = FileAllocationTable[cur_index];
		free(cur_path);
	} while (cur_index != 0xFFFF);
	fclose(fp);
}

/* Prints out occupied indices in the FAT in a readable format.
 */
void printFAT() {
	cout << "Printing occupied entries in FAT table" << endl;
	for (int i = 0; i < cluster_size; i++) {
		int index = FileAllocationTable[i];
		// If non-empty, print out index it points to
		if (index != 0) {
			cout << i << ": " << index << endl;
		}
	}
}

/* Returns an int that represents a cluster that is available for use within the system.
 */
int findAvailableCluster() {
	// Iterate across the FAT and if an entry is empty, return it
	for (int i = 1; i < num_clusters; i++) {
		// Make sure it's empty, not at the FAT index, and not at the root directory table index.
		// The only time the last two conditions are true are if no file exists in the system.
		// I'm actually not sure if that happens anymore, though. Looking at it, it doesn't make much sense now.
		if (FileAllocationTable[i] == 0x0000 && i != FAT_index && i != root_index && i != 0xFFFE) {
			return i;
		}
	}
	// Return -1 if there are no available clusters.
	return -1;
}

/* Update the FAT at a given index, write the data, and return an available index.
 * int writeIndex - the index that is being updated
 */
int writeFATRecord(int writeIndex) {
	// Find an available cluster, update the FAT, and return the available cluster index
	int nextIndex = findAvailableCluster();
	FileAllocationTable[writeIndex] = nextIndex;
	FileAllocationTable[nextIndex] = 0xFFFF;
	return nextIndex;
}

/* Reads the data from a specified cluster into a character array.
 * int clusterIndex - the index of the cluster that is going to be read
 * char* data - pointer to the character array in which the data will be stored
 */
void readCluster(int clusterIndex, char* &data) {
	char* cur_path = get_current_dir_name();
	chdir(fs_dir);
	// Open the file, read in a cluster of data, then close the file
	FILE* fp = fopen(fs_name, "r");
	fseek(fp, clusterIndex * cluster_size, SEEK_SET);
	fread(data, cluster_size, 1, fp);
	fclose(fp);
	chdir(cur_path);
	free(cur_path);
}

/* Writes a character array of data to a specified cluster.
 * int clusterIndex - the index of the cluster that is going to be written
 * char* data - pointer to the character array in which the data is stored
 * unsigned int size - the number of bytes that are going to be written
 */
void writeCluster(int clusterIndex, char* &data, unsigned int size) {
	char* cur_path = get_current_dir_name();
	chdir(fs_dir);
	// Open the file, write the cluster of data, then close the file
	FILE* fp = fopen(fs_name, "r+");
	fseek(fp, clusterIndex * cluster_size, SEEK_SET);
	fwrite(data, size, 1, fp);
	fclose(fp);
	chdir(cur_path);
	free(cur_path);
}

/* Finds an available entry in the directory table 
 */
int findAvailableEntry() {
	directory_entry* cur_entry = (directory_entry*)malloc(128);
	directory_entry* table = (directory_entry*)malloc(cluster_size);
	int cur_index = root_index;
	while(true) {
		FILE* fp = fopen(fs_name, "r");
		fseek(fp, cur_index * cluster_size, SEEK_SET);
		fread(table, cluster_size, 1, fp);
		fclose(fp);
		// Find an available entry and return its absolute index in the file system
		for (int i = 0; i < cluster_size / 128; i++) {
			cur_entry = &table[i];
			if (cur_entry->name[0] == 0xFF || cur_entry->name[0] == 0x00) {
				return i * sizeof(directory_entry) + cur_index * cluster_size;
			}
		}
		if (FileAllocationTable[cur_index] == 0xFFFF) break;
		cur_index = FileAllocationTable[cur_index];
	}
	// If no entry is found, then make a new directory table
	// If a directory table cannot be created, return -1
	// Otherwise return the index of the new directory table
	int new_index = writeFATRecord(cur_index);
	if (new_index == -1) return new_index;
	else return new_index * cluster_size;
}

/* Updates the root directory table of the file system.
 */
void updateDT() {
	FILE* fp = fopen(fs_name, "r");
	fseek(fp, root_index * cluster_size, SEEK_SET);
	fread(dir_table, cluster_size, 1, fp);
	fclose(fp);
}

/* Updates the FAT of the file system.
 */
void updateFAT() {
	FILE* fp = fopen(fs_name, "r");
	fseek(fp, FAT_index * cluster_size, SEEK_SET);
	fread(FileAllocationTable, cluster_size, 1, fp);
	fclose(fp);
}

/* Writes the FAT to the file containing the file system.
 */
void writeFAT() {
	FILE* fp = fopen(fs_name, "r+");
	fseek(fp, FAT_index * cluster_size, SEEK_SET);
	fwrite(FileAllocationTable, cluster_size, 1, fp);
	fclose(fp);
}
