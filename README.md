# file-archiver
This is a program that creates a malleable archive. The program has 4 main functions: adding/updating a file in the archive with all its contents (r), extract a file from the archive to create a real file in the folder based on the archived version (x), delete a file from the archive (d), and print the archive out (t).

The inspiration from this project came from a class I didn't get to take, but seemed interesting to me at the time. It's functionality is seemingly simple, but it was difficult in that it forced me to work at a really low level. I wanted to take on a project that would challenge me and make me a better programmer. It's a few hundred lines (~700) of C code. 

You can run this code as follows:
./farthing key(r/x/d/t) nameOfArchive [name]*
where the first word runs the program, the second is a given function call as described above, the third is the name of the file you want to be the archive, and the fourth and onwards are files you want to delete, extract, etc.
