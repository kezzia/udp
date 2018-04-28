To compile and run the server:
gcc -o server server.c
./server <port num> <loss ratio (float)> <random seed (int)>

To compile and run the client:
gcc -o client client.c
./client <ip addr> <port num> <input file> <to format> <output file>
<loss ratio (float)> <random seed(int)>

Known problems:
- Text having trouble writing to output file
- Sometimes the packet will just never send. In order to prevent this,
we give the packet 6 chances to send before ending the Program
- As we discussed in your office, the program reads in ascii instead of binary,
so please use the included test file so it doesn't crash
