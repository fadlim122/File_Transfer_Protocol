#include <stdlib.h>
#include <string.h>
#include "token.h"

int tokenise(char *inputLine,char *token[])
{
	int i=0;
	char *temp;


	temp=strtok(inputLine," \t\n");

	while(temp!=NULL && i<MAX_NUM_TOKENS)
	{
		token[i]=temp;
		temp=strtok(NULL," \t\n");
		i++;
	}

	if(i==MAX_NUM_TOKENS &&temp!=NULL)
	{
		return -1;
	}else if(i==0){
		return -2;
	}else{
		return i;
	}
}

