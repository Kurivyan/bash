#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_func(){
	printf("\e[1;96m");
	int size = 20;
	char* ptr = calloc(size, sizeof(char));
	char c;
	c = getchar();

	int i = 0;
	int flag = 0;
	int quotes = 0;

	while(c != '\n'){
		if (i == size - 1){
			size += 10;
			ptr = realloc(ptr, size * sizeof(char));
		}

		if(c != ' ' && flag == 1)
			flag = 0;

		if(c == ' ' && flag == 1){
			c = getchar();
			continue;
		}

		if(c == ' ' && flag == 0){
			if(quotes != 1){
				flag = 1;
			}
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		if(c == ' ' && i == 0){
 			c = getchar();
			continue;
		}

		if(c == '\"' && quotes == 0){
			quotes = 1;
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		if(c == '\"' && quotes == 1){
			quotes = 0;
			ptr[i] = c;
			i++;
			c = getchar();
			continue;
		}

		ptr[i] = c;
		i++;
		c = getchar();
	}
	ptr[i] = '\0';

	if(*ptr == '\0'){
		free(ptr);
		return NULL;
	}
	printf("\e[m");
	return ptr;
}

#include "reader.c"