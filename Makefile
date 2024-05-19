all: part1 part2 part3 part4

part1:
	gcc execute.c splitline.c splitline2.c smsh2.c -std=c99 -Wall -o smsh2 
part2:
	gcc execute.c splitline.c splitline2.c smsh3.c -std=c99 -Wall -o smsh3
part3:

clean:
	rm -f smsh2 smsh3 smsh4