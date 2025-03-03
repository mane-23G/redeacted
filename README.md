# redacted
This is a parallel algorithm that uses threads to find all occurences of a particular string in a text and replace it with the thread id.

### Build with
gcc -o redacted redacted.c

### Usage 
redacted \<num of threads\> \<pattern\> \<input file\> \<output file\>

The user must the number of threads it would like the program to use, the pattern to search for and the file that contains the text to search for the pattern in, and finally an output file where the redacted string will be written to.

### Output and Features
The string is written to a file provided by the user
If two threads get segements that overlap, the redaction is determined by precednce. There is 64 redaction characters that get assigned to each thread accordingly.

### Defects/Shortcomings
None

Thank you to Professor Weiss for the assigment! b
