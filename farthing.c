//   System header files and macros for farthing from Stan Eisenstat
#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>

// Write message to stderr using format FORMAT
#define WARN(format,...) fprintf (stderr, "farthing: " format "\n", __VA_ARGS__)
#define INPUT_FORMAT "farthing r|x|d|t archive [name]*"

// Write message to stderr using format FORMAT and exit.
#define DIE(format,...) WARN(format,__VA_ARGS__), exit (EXIT_FAILURE)

// Change allocated size of array X of unspecified type to S elements.
// X may be NULL; assumes that realloc() never fails.
#define REALLOC(x,s) x = realloc (x, (s) * sizeof(*(x)))

struct fileStruct {
	char *name;
	struct stat buffer;
	int fd;
	int type;
	int shouldChange;
	struct fileStruct *next;
};


//function that passes a name and a buffer and checks for file type
int fileType(char const *fileName, struct stat *buffer){
	//printf("We're in fileType\n");
	//check if file exists
    int doesExist = lstat(fileName, buffer);
    if(!doesExist) {//lstat was successful and thus returned 0 
		
    	//checks for the filetype by unmasking the mode
    	if((buffer->st_mode & S_IFMT) == S_IFREG) { //checks that it's a regular file
    		//printf("your archive is a regular file\n");
    		return 1;
    	} else if ((buffer->st_mode & S_IFMT) == S_IFDIR){ //directory
    		//printf("your archive is a regular directory\n"); 
    		return 2;
    	} else { //other file type
    		WARN("%s already exists as a non-regular file \n", fileName); //throw an error for non-regular file
    		return -1; 
    	}
    }
    else {// not a file/does not exist
        return 0;
    }
}

int checkArchive(char const *archive, int isR){
	//printf("We're in checkArchive\n");
	struct fileStruct archiveStruct;
    int existsArchive = fileType(archive, &archiveStruct.buffer); //access the archive
    int cannotRead = access(archive, R_OK);
    if (!isR && ((existsArchive != 1) || cannotRead)) { //not the right file type, not yet existing, or no read access
    	DIE("%s is not a readable archive file", archive);
    }
    return existsArchive;
}

void deleteLinkedList(struct fileStruct * file){
	//printf("We're in deleteLinkedList\n");
	if (file != NULL) {
		free(file->name); //free name
		deleteLinkedList(file->next); //free next node
		free(file); //free this one
	}
}


void clearAll(FILE *archive, FILE*temp, struct fileStruct *linkedList){
	//printf("We're in clearAll\n");
	if (archive != NULL) fclose(archive);
	if (temp != NULL) fclose(temp);
	deleteLinkedList(linkedList);
}

//makes a name into a directory name terminated in '/'
char * makeName (const char * oldName, int len, int fileType){
	//printf("We're in makeName\n");
	//printf("The old name is %s\n", oldName);
	char * newName = calloc(len + 2, sizeof(char));
	strncpy(newName, oldName, len);
	//printf("Old Name: %s New name: %s\n", oldName, newName);
	if (fileType == 2) { //directory
		if (newName[len-1] != '/') { //if last char of a dir name isn't '/'
			newName[len] = '/'; //make it so
		} else {
			for (int i = (len - 1); i > 0; i--){
				if ((newName[i] == '/') && (newName[i-1] == '/')){
					newName[i] = '\0';
				}
			}
		}

	}
	//printf("Calloc'ed in makeName for %s\n", newName);
	return newName;
}

struct fileStruct * makeFileStruct(const char *name, int inDir) {
	//printf("We're in makeFileStruct\n");
	//printf("The name for the fileStruct is: %s\n", name);
	struct fileStruct *file = calloc (1, sizeof(struct fileStruct)); 
	//printf("Calloc'ed for fileStruct for %s\n", name);
	file->fd = lstat(name, &file->buffer); //load file info into buffer
	file->type = 1;
	if(inDir){
		file->type = fileType(name, &file->buffer); //get the fileType
	}
	file->next = NULL;

	int len = strlen(name);
	file->name = makeName(name, len, file->type);
	
	file->shouldChange = 1;
	return file;
}


//takes in a file and adds it to the end of a linked list and returns the node
struct fileStruct * addToLinkedList(struct fileStruct * last, char * name, int isR){
	last->next = makeFileStruct(name, isR);
	return last->next;
}

char *fullFileName (char * dirName, char * fileName){
	//printf("We're in fullFileName\n");
	char * fullName = calloc((strlen(dirName) + strlen(fileName) + 1), sizeof(char)); 
	strcpy(fullName, dirName);
 	strcat(fullName, fileName); //CHECK IF LAST VALUE IS A NULL TERMINATING CHAR
 	return fullName;
}


struct fileStruct * makeLinkedListRecursive(struct fileStruct * current, char * dirName){
	DIR *d;
	struct dirent *dir;
	d = opendir(dirName);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")){
				continue;
			}
			char *name = fullFileName(dirName, dir->d_name);
			current = addToLinkedList(current, name, 1);

			if (current->type == 2){
				current = makeLinkedListRecursive(current, current->name); 
			}
			free(name);
		}
	}
	closedir(d);
	return current;
}

//makes the linked list and returns the first node
struct fileStruct * makeLinkedList(const char *fileNames[], int len, int isR){
	//printf("We're in makeLinkedList\n");
	struct fileStruct * first = makeFileStruct(fileNames[0], isR);
	struct fileStruct *temp = first;
	if (isR && (temp->type ==2)) { //for replace
		temp = makeLinkedListRecursive(temp, temp->name);
	}
	
	for (int i = 1; i < len; i++){
		temp = (addToLinkedList(temp, (char *) fileNames[i], isR));
		if (isR && (temp->type == 2)) {
			temp = makeLinkedListRecursive(temp, temp->name);
		}
	} 
	return first;
}

void printLinkedList(struct fileStruct *file){
	if(file != NULL){
		printf("%s %d %d\n", file->name, file->shouldChange, file->type);
		printLinkedList(file->next);
	}
}

//function to check line with filestructs and returns a pointer to the file with a matching name
struct fileStruct * checkExistingFiles(char * nameInArc, FILE *archive, FILE *newArchive, struct fileStruct *firstNode, char * newArchName){
	//printf("We're in checkExistingFiles\n");
	int nameInArcLen = strlen(nameInArc); 
	int typeInArc = 0; //holds the fileType of the archival name

	//check for file type of the archived file
	if (nameInArcLen <= 0) {
		unlink(newArchName);
		fclose(archive);
		deleteLinkedList(firstNode);
		DIE("Nonpositive name length %d. Incorrect archive format.", nameInArcLen);
		return NULL;
	} else {
		typeInArc = 1; //a reg file
	}
	
	//printf("%c SECOND TO LAST \n", nameInArc[nameInArcLen - 1]);
	if (nameInArc[nameInArcLen - 1] == '/') {
		typeInArc = 2;
	}

	//iterate through fileName list to check if it exists
	for (struct fileStruct *temp = firstNode; temp != NULL; temp = temp->next){
		if((strlen(temp->name) == nameInArcLen) && (strcmp(nameInArc, temp->name) == 0)){
			//if fileTypes differ 
			if (temp->type != typeInArc) {
				temp->shouldChange = -2;
			} else {
				return temp;
			}
		}
	}
	return NULL; 	//doesn't match any name so it's not being replaced
}

//gets the name from (ideally) a header line in the archive file --> or just the first string
char * getName(FILE *archive){ //WHAT HAPPENS WHEN A NAME DOESN'T EXIST?
	//printf("We're in getName\n");
	int len;
	char pipe;
	if ((fscanf(archive, "%d %c", &len, &pipe)) == EOF) {
		return NULL;
	}
	//printf("WHAT IS LEN HERE? %d ", len);
	char * name = calloc((len +1), sizeof(char));
	fscanf(archive, "%s", name);
	//printf("name: %s\n", name);
	return name;
}

//moves numOfChars chars over from one file to another
void copyContents(int numOfChars, FILE *oldArc, FILE *newArc){
	//printf("We're in copyContents\n");
	for (int i = 0; i<numOfChars; i++){
		int c = fgetc(oldArc);
		fputc(c, newArc); //takes char from file and inserts it into the archive
		//printf("%c", c);
	}
}

void insertFileIntoArchive(FILE *ogFile, FILE *archive, int nameSize, char* name, int contentSize){
	fprintf(archive, "%d|%s\n", nameSize, name);
	//printf("%d|%s\n", nameSize, name);
	fprintf(archive, "%d|", contentSize);
	//printf("%d|", contentSize);
	if (!(name[nameSize - 1] == '/')) { //if not an archive (aka if the last char isn't a '/')
		copyContents(contentSize, ogFile, archive);
	}
}

//fseek FINISH WRITING
int myFSeek(FILE *archive, int num){
	//printf("We're in myFSeek\n");
	int c;
	for (int i = 0; i < num; i++){
		c = fgetc(archive);
	}
	return c;
}

void replace(int numOfFiles, char const *fileNames[], char const *archiveName){
    //check archive existence
	FILE *archive = NULL;
	FILE *tempArch = NULL;
	int existsArchive = 0;
	char *tempArchName = "";
	if(!strcmp(archiveName,"-")){
		archive = stdin; 
		existsArchive = 1;
		tempArch = stdout;
	} else {
		existsArchive = checkArchive(archiveName, 1);
		archive = fopen(archiveName, "r"); //open archive for reading 
		if (existsArchive && access(archiveName, R_OK)){
			fclose(archive);
			DIE("%s cannot be opened.", archiveName);
		}
		tempArchName = "ARCHIVE.bak";
		tempArch = fopen(tempArchName, "w");
	}

	//make data structure for files
	struct fileStruct *linkedList = makeLinkedList(fileNames, numOfFiles, 1); //create the linked list of files
	for (struct fileStruct *file = linkedList; file != NULL; file = file->next){
		if ((file->type != 1) && (file->type != 2)) { //regular file
			file->shouldChange = -1; //file doesn't exist or not in the right format!
		} 
	}

	//get the name
	char *name;
	//go through every line of the archive while there are still lines of the archive
	while ((existsArchive != 0) && //if the archive already exists
		  ((name = getName(archive)) != NULL)) { //gets the name of a file from the next header in an archive
		//check the name among the files
		struct fileStruct *fileToInsert = checkExistingFiles(name, archive, tempArch, linkedList, tempArchName);

		//if it exists and is a regular file, 
		int contentSize;
		if (fileToInsert != NULL){ //if in the linked list and should insert
			fscanf(archive, "%d", &contentSize); //get the first number
			myFSeek(archive, contentSize+1); //skip over the file contents within the archive
		} else if (fileToInsert == NULL){ //if it isn't in the linked list
			int nameSize = strlen(name);
			fscanf(archive, "%d|", &contentSize); //get contentSize 
			insertFileIntoArchive(archive, tempArch, nameSize, name, contentSize); //puts header and contents into new archive
		}
		free(name);
	}
	
	//print into the new archive the files that haven't been put into the archive yet
	for (struct fileStruct *file = linkedList; file != NULL; file = file->next){
		if (file->shouldChange == 1) {
			//get info about the file 
			FILE * regFile = fopen(file->name, "r");
			int fileNameLen = strlen(file->name);
			int fileSize = file->buffer.st_size; 
			if (file->type == 2){
				fileSize = 0;
			}
			//insert the header and contents(if applicable)
			insertFileIntoArchive(regFile, tempArch, fileNameLen, file->name, fileSize);
			file->shouldChange = 0;
			fclose(regFile);
		}
	}

	//4) if any file wasn't inserted yet, and so shouldInsert == 1 still, throw warnings
	for (struct fileStruct * file = linkedList; file != NULL; file = file->next){
		if (file->shouldChange == 1) {
			WARN("%s was not inserted once.", file->name);
		} else if (file->shouldChange < 0){
			WARN("Error: %d. %s could not be inserted.", file->shouldChange, file->name);
		}
	}

	//close all open files and delete the data structure for files
	clearAll(archive, tempArch, linkedList);
	if (strcmp(archiveName,"-")){
		rename(tempArchName, archiveName);
	}
}

//takes in a name and a list of files to change and outputs the matching file struct
struct fileStruct * checkName(struct fileStruct *linkedList, char * nameInArc){
	int len;
	for (struct fileStruct *temp = linkedList; temp != NULL; temp = temp->next){ //go through the linked list and sees if the name is in the linkedList
		len = strlen(temp->name); //get the length of the inputted name from user --> would be <= files beginning with that name in the archive
		char * dirName = makeName(temp->name, len, 2); //makes a version of the name that is the name of a directory, like name/
		int inDir = strncmp(nameInArc, dirName, len+1);
		int matchExactName = strcmp(nameInArc, temp->name);

		//CHECK IF YOU HAVE TO GO THROUGH ENTIRE LINKED LIST AND CHANGE MAKE SHOULDCHANGE=0 FOR EVERY MATCH OR JUST FIRST
		if(!inDir || !matchExactName){ //in directory hierarchy with that name or matches name
			free(dirName);
			return temp;
		}
		free(dirName);
	}
	return NULL;
}

//prints out the name and size of one file as read in by the archive
int printOne (FILE *archive, struct fileStruct * linkedList) {//in more complex version, include (struct fileStruct *linkedList, char * name) in parameters
	char * fileInArc = getName(archive);
	if (fileInArc != NULL){ //if we didn't hit the end of file (would get no name)
		if(linkedList == NULL) { // if we're printing all files in the archive
			int num;
			fscanf(archive, "%d", &num);
			printf("%8d %s\n", num, fileInArc); 
			myFSeek(archive, num +1);
			free(fileInArc);
			return 1;
		} else { // if we're printing only select files
			struct fileStruct * temp = NULL;
			int num;
			fscanf(archive, "%d", &num); 
			myFSeek(archive, num + 1);
			if ((temp = checkName(linkedList, fileInArc)) != NULL){ //if the name is one of the inputted names
				temp->shouldChange = 0;
				printf("%8d %s\n", num, fileInArc);
			}
			free(fileInArc);
			return 1;
		}
	}
	return 0;
}

void printArchive(int argc, char const *argv[]){
	//check archive existence
	FILE *archive;
	if(!strcmp(argv[2],"-")){
		archive = stdin; 
	} else {
		checkArchive(argv[2], 0);
		archive = fopen(argv[2], "r"); //open archive for reading 
	}
    
	int printAll = 0;
	if (argc==3) {
		printAll = 1;
	}

	struct fileStruct * linkedList = NULL;
	if (!printAll) { //if we're only printing some
		linkedList = makeLinkedList(&argv[3], argc - 3, 0); //puts in the pointer to the args that are filenames and the number of files (total # of args - 3)
	}

	int keepPrinting = 1;
	while (keepPrinting == 1){
		keepPrinting = printOne(archive, linkedList);
	}

	//check that everything printed
	for (struct fileStruct * file = linkedList; file != NULL; file = file->next){
		if (file->shouldChange == 1) {
			WARN("%s was not printed once.", file->name);
		} else if (file->shouldChange < 0){
			WARN("Error: %d. %s could not be printed.", file->shouldChange, file->name);
		}
	}
	//close all open files and delete the data structure for files
	clearAll(archive, NULL, linkedList);
}

void delete(char const * archiveName, char const *fileNames[], int numFiles){
	//check archive existence
	FILE *archive = NULL;
	FILE *tempArch = NULL;
	char *tempArchName = "";
	if(!strcmp(archiveName,"-")){
		archive = stdin; 
		tempArch = stdout;
	} else {
		checkArchive(archiveName, 0);
		archive = fopen(archiveName, "r"); //open archive for reading 
		tempArchName = "ARCHIVE.bak";
		tempArch = fopen(tempArchName, "w");
	}

	//make data structure for files
	struct fileStruct *linkedList = makeLinkedList(fileNames, numFiles, 0); //create the linked list of files

	//get the name
	char *name;
	//go through every line of the archive while there are still lines of the archive
	while ((name = getName(archive)) != NULL) { //gets the name of a file from the next header in an archive

		//check the name among the files
		struct fileStruct *fileToInsert = checkName(linkedList, name);

		//if it exists and is a regular file, 
		int contentSize;
		if (fileToInsert != NULL){ //if in the linked list and should delete
			fscanf(archive, "%d", &contentSize); //get the first number
			myFSeek(archive, contentSize+1); //skip over the file contents within the archive
			fileToInsert->shouldChange = 0;
		} else if (fileToInsert == NULL){ //if it isn't in the linked list
			int nameSize = strlen(name);
			fscanf(archive, "%d|", &contentSize); //get contentSize 
			insertFileIntoArchive(archive, tempArch, nameSize, name, contentSize); //puts header and contents into new archive
		}
		free(name);
	}

	//if any file wasn't deleted yet, and so shouldInsert == 1 still, throw warnings
	for (struct fileStruct * file = linkedList; file != NULL; file = file->next){
		if (file->shouldChange == 1) {
			WARN("%s was not deleted once.", file->name);
		} 
	}

	//close all open files and delete the data structure for files
	clearAll(archive, tempArch, linkedList);
	if (strcmp(archiveName,"-")){
		rename(tempArchName, archiveName);
	}
}

//given an original name and the index of the "/", this function returns the name of just the directory
char * directoryName(char * fullName, int index){
	//printf("We're in directoryName\n");
	char * name = calloc((index+2), sizeof(char));
	strncpy(name, fullName, index+1);
	return name;
}

//given a file name and the index of a substring, this returns the index of any subsequent "/"
int findNextIndex(char * fullName, int currentIndex){
	//printf("We're in findNextIndex\n");
	char *indexPtr = strchr(&fullName[currentIndex], '/'); //gets index of '/'
	int index;
	if (indexPtr == NULL){
		index = -1;
	} else {
		index = indexPtr - fullName;
	}
	//printf("string: %s index:%d\n", &fullName[currentIndex-1], index);
	return index;
}

//if a file name passed in is a directory, and thus ends in '/', this function returns a non-directory form of the name
char * makeSimpleName(char * oldName, int len){
	//printf("We're in makeSimpleName\n");
	//printf("The old name is %s\n", oldName);
	char * newName = NULL;
	//printf("Old Name: %s New name: %s\n", oldName, newName);
	if (oldName[len-1] == '/') { //if last char of a dir name is '/'
		newName = calloc(len, sizeof(char));
		strncpy(newName, oldName, len-1);
	}
	return newName;
}

//assuming the file's directory exists, extract the file from the archive into the directory
int extractOne(char *name, FILE * archive, int fileType, int contentSize){
	//printf("We're in extractOne\n");
	//make a fileStruct for the name
	//printf("The name in ExtractOne is: %s\n", name);
	struct fileStruct * file = makeFileStruct(name, 1);

	//check edge cases
	if (file->type){
		//if file exists but it's not the right file type OR if the filetypes don't align
		if ((file->type < 0) || (fileType != file->type)){
			deleteLinkedList(file);
			WARN("Incompatible file type for %s. File type is : %d and wanted filetype is: %d", name, file->type, fileType);
			return -1;
		} 

		//check if you can write into the file/directory --> should have write access
		int cannotWrite = access(name, W_OK);
		if (cannotWrite){
			deleteLinkedList(file);
			WARN("No write access to %s", name);
			return -1;
		}
	}
				
	//if the file should be a regular file, make or overwrite the file and copy contents from archive into it
	if (fileType == 1) {
		char *tempFileName = "ARCHIVE.bak";
		FILE *tempFile = fopen(tempFileName,"w");
		copyContents(contentSize, archive, tempFile);
		fclose(tempFile);
		rename(tempFileName, name);
	} else if (fileType == 2) { //if it should be a directory, make the directory
		if (!file->type){ //if the file doesn't already exist
			mkdir(name, S_IRWXU | S_IRWXG | S_IRWXO); //make the directory
		}
	} else {
		WARN("Something went wrong for %s.", name);
		return -1;
	}	

	deleteLinkedList(file);
	return 0;
}

//using the name from an archive file, create all necessary 
int extractOneDir(char * name, FILE *archive){
	//printf("We're in extractOneDir\n");
	//get the size of the contents and the pipe from the archive
	int contentSize;
	char pipe;
	fscanf(archive, "%d%c", &contentSize, &pipe);

	int len = strlen(name);
	char *tempName;
	for(int index = 0; index != -1 && index < (len-1);){ //until index is out-of-bounds or at the end of the string
		//from the file name, get the index of any unchecked directory name within it, if any
		index = findNextIndex(name, index + 1);
		
		if (index != -1){ //if a directory is brought back
			tempName = directoryName(name, index); //using that index, get the filename of the directory
		} else { //otherwise use the name passed in
			tempName = name;
		}

		char* shortName = makeSimpleName(tempName, strlen(tempName)); //get a version of the name without "/" --> works only for directories
		int didExtract = 0;
		if (shortName) { //if it's a directory
			//printf("We're in the directory case for name %s and shortName %s\n", name, shortName);
			didExtract = extractOne(shortName, archive, 2, contentSize);
			free(shortName);
		} else { //it's a regular file
			//printf("We're in the regular file case where the name %s is extracted", name);
			didExtract = extractOne(name, archive, 1, contentSize);
		}

		if (index != -1) { //free the tempname created for a directory
			free(tempName);
		}

		if (didExtract == -1){
			myFSeek(archive, contentSize); //Use myFSeek to move past content
			return -1; //return an error so that the parallel file in the linked list can be should->change = 0;
		}
	}

	return 0;
}

//extracts all files in the archive
void extractAll(FILE *archive){
	//printf("We're in extractAll\n");
	//while you can get name from the archive
	char * name;
	while((name = getName(archive)) != NULL) {
		extractOneDir(name, archive);
		free(name);
	}

}

//extracts only the files inputted by user
void extractSome(const char *fileNames[], int numFiles, FILE * archive){
	//printf("We're in extractSome\n");
	//make a linked list from the names in extract
	struct fileStruct * linkedList = makeLinkedList(fileNames, numFiles, 0);
	//while loop for the archive
	char * name;
	while((name = getName(archive)) != NULL) { //get the name
		struct fileStruct * file = checkName(linkedList, name);
		if (file){
			extractOneDir(name, archive);
			file->shouldChange = 0;
		} else {
			int contentSize;
			char pipe;
			fscanf(archive, "%d%c", &contentSize, &pipe);
			myFSeek(archive, contentSize);
		}
		free(name);
	} 

	//check and throw error if a file wasn't inserted yet
	for (struct fileStruct * file = linkedList; file != NULL; file = file->next){
		if (file->shouldChange == 1) {
			WARN("%s was not extracted once.", file->name);
		} 
	}

	deleteLinkedList(linkedList);
}

void extract(int numFiles, char const *fileNames[], char const *archiveName){
	//check that archive exists
	FILE *archive;
	if(!strcmp(archiveName, "-")){
		archive = stdin; 
	} else {
		checkArchive(archiveName, 0);
		archive = fopen(archiveName, "r"); //open archive for reading 
	}
	
	//extract all or one by one
	if (numFiles == 0) { //no names means extract all
		extractAll(archive);
	} else {
		extractSome(fileNames, numFiles, archive);
	}
	
	//free everything properly
	fclose(archive);
}

int main(int argc, char const *argv[]) {
	if (argc < 3) {
		DIE("input should follow the following format: %s", INPUT_FORMAT); //throws an error
	} else {
		int numFiles = argc - 3;
		//check key is one of the commands with a switch case
		switch (*argv[1]) { //DOUBLE CHECK IF POINTER OR NOT
			case 'r':
				if (argc < 4) {
					DIE("input should follow the following format: %s", INPUT_FORMAT); //throws an error
				}
				//printf("Replace says r\n");
				replace(numFiles, &argv[3], argv[2]);
				break;
			case 'x':				
				//printf("Extract says x \n");
				extract(numFiles, &argv[3], argv[2]);
				break;
			case 'd':
				//printf("Delete says d \n");
				delete(argv[2], &argv[3], argc-3);
				break;
			case 't':
				//printf("Print says t \n");
				printArchive(argc, argv);
				break;
			default:
				WARN("%s isn't a valid value.", argv[1]);
				exit(0);
		}
	return 0;
	}
}

//EDGE CASES
//number of error messages = actual errors
//number of fopens only for files, not directories (???)
//5 criteria: chmod/changed file permissions, directories that turn into files, files that turn into directories, if the name is just slashes or a letter followed by many slashes (linux says its not valid)
//check memory usage
//check really long filenames
//make local tests (with files with open) --> look at what the tests do and mimic it in your local tests
//other brainstorming issues: discrepancies between system and archive 
//all the formats you need for the archive mimics/reflects the reality (ex. that length in "header" doesn't match content length)
//if it's read in from stdin
//even if there's the same name, if there's multiple warnings for same file/issue, issue error each time
//check the sizes of all the ints
