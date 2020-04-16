/* myftp.c
 *          This is improved on the base "cli6.c".
 *          This is the client program for Project 2
 *  Revised;	17/11/2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>       /* struct sockaddr_in, htons, htonl */
#include <netdb.h>            /* struct hostent, gethostbyname() */
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include "stream.h"           /* MAX_BLOCK_SIZE, readn(), writen() */
#include "token.h"
#include <fcntl.h>

#define SERV_TCP_PORT  40145 /* default server listening port */
#define BUFSIZE 16

void handler_ldir();
void handler_lcd(char path[]);
void handler_lpwd();
void handler_cd(int sd, char path[]);
void handler_dir(int sd);
void handler_pwd(int sd);
void option(char *token[], int tokenNo,int sd);
void handler_put(int sd,char filename[]);
void handler_get(int sd,char filename[]);

/* main function */
int main(int argc, char *argv[])
{
     int sd, n, nr, nw, i=0,tokenNo;
     char buf[MAX_BLOCK_SIZE], host[60],*token[MAX_NUM_TOKENS];
     unsigned short port;
     struct sockaddr_in ser_addr; struct hostent *hp;

     /* get server host name and port number */
     if (argc==1) {  /* assume server running on the local host and on default port */
          gethostname(host, sizeof(host));
          port = SERV_TCP_PORT;
     } else if (argc == 2) { /* use the given host name */
          strcpy(host, argv[1]);
          port = SERV_TCP_PORT;
     } else if (argc == 3) { // use given host and port for server
         strcpy(host, argv[1]);
          int n = atoi(argv[2]);
          if (n >= 1024 && n < 65536)
              port = n;
          else {
              printf("Error: server port number must be between 1024 and 65535\n");
              exit(1);
          }
     } else {
         printf("Usage: %s [ <server host name> [ <server listening port> ] ]\n", argv[0]);
         exit(1);
     }
    /* get host address, & build a server socket address */
     bzero((char *) &ser_addr, sizeof(ser_addr));
     ser_addr.sin_family = AF_INET;
     ser_addr.sin_port = htons(port);
     if ((hp = gethostbyname(host)) == NULL){
           printf("host %s not found\n", host); exit(1);
     }
     ser_addr.sin_addr.s_addr = * (u_long *) hp->h_addr;

     /* create TCP socket & connect socket to server address */
     sd = socket(PF_INET, SOCK_STREAM, 0);
     if (connect(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr))<0) {
          perror("client connect"); exit(1);
     }

     // connect with server
    	while(1){
        printf("\nPlease enter your command\n");
    		printf("$ ");

    		fgets(buf,sizeof(buf),stdin);
    		tokenNo=tokenise(buf,token);

    		if(tokenNo==-1)
    		{
    			printf("[Error]: Input has exceeded the limit.\n");
    		}else if(tokenNo==-2)
    		{
    			printf("[Error]:No words or tokens are being detected.\n");
    		}else if(tokenNo>=1&&tokenNo<=2){
    			option(token,tokenNo,sd);
    		}
      }
    	exit(0);
}

/* handle the user input*/
void option(char *token[], int tokenNo,int sd){
	if(tokenNo == 1){
		if(strcmp(token[0],"pwd")==0){
      handler_pwd(sd);
		}else if(strcmp(token[0],"lpwd")==0){
	    handler_lpwd();
		}else if(strcmp(token[0],"dir")==0){
      handler_dir(sd);
		}else if(strcmp(token[0],"ldir")==0){
	    handler_ldir();
		}else if(strcmp(token[0],"quit")==0){
      char c_code = 'Q';
      writen(sd,&c_code,1);
		 	exit(0);
		}
	}else if(tokenNo==2){
		if(strcmp(token[0],"cd")==0){
	    handler_cd(sd, token[1]);
		}else if(strcmp(token[0],"lcd")==0){
	    handler_lcd(token[1]);
		}else if(strcmp(token[0],"get")==0){
			handler_get(sd,token[1]);
      // ReceiveFile(sd, token[1]);
		}else if(strcmp(token[0],"put")==0){
			handler_put(sd,token[1]);
		}
	}else{
	  printf("[Client]: Something is wrong with the command.\n");
	}
}

/* handle command put*/
void handler_put(int sd,char filename[]){
  char c_code = 'A';
  char s_code;
	int filelen;
  int fd;

  printf("[Client]: File name: %s\n", filename);

  // send put command code to server
  if(writen(sd, &c_code, 1) < 0){
    printf("[Client Error]: Cannot send command to server\n");
    return;
  };

  //Send file name to be uploaded to server
  filelen=strlen(filename);
	writen(sd,(char *)&filelen,2);
	writen(sd, filename,filelen);

  // read server feedback code
	readn(sd, &s_code,1);
  printf("S_CODE: %c\n", s_code);
	if(s_code == '1'){
		printf("[Server]: The server is ready to accept the file.\n");
		int contentlength;

    // open file
		if((fd = open(filename, O_RDONLY)) == -1)
		{
      printf("[Client Error]: File does not exist.\n");
      return;
    }
    // caculate file size and send to server
		int file_size;
    file_size = lseek(fd, 0, SEEK_END);
    printf("[Client]: File size: %d", file_size);
    lseek(fd, 0, SEEK_SET);
		writen(sd,(char *)&file_size,4);

		char block[MAX_BLOCK_SIZE];
    int nr;
    // sending file to server
    while((nr = read(fd, block, MAX_BLOCK_SIZE)) > 0)
    {
       if(writen(sd, block, nr) == -1)
       {
           printf("[Client Error]: Error sending file.\n");
           return;
       }
    }
		printf("[Client]: Upload completed.\n");
    memset(block,0,MAX_BLOCK_SIZE);
		close(fd);
    fflush(stdout);
	}else if(s_code == '2'){
		printf("[Server Error]: Duplicated name.\n");
	}else if(s_code == '0'){
		printf("[Server Error]: File cannot be created.\n");
	}else{
    printf("[Server Error]: Wrong code.\n");
  }
}

/* handle get command (download file) */
void handler_get(int sd,char filename[]){
 int fd;
 char c_code = 'B', s_code;
 int filelen;

 printf("[Client]: Filename: %s\n", filename);

 // check if the file wheter exited or not
 if((fd = open(filename, O_RDONLY)) == -1)
 {
  // send get command code to server
  if(writen(sd, &c_code,1) < 0){
    printf("[Client Error]: Cannot send command to server\n");
    return;
  };
  // send the file name to server
  filelen=strlen(filename);
	writen(sd,(char *)&filelen,2);
	writen(sd,filename,filelen);

  // read server feedback code
	readn(sd,&s_code,1);
	if(s_code == '0'){
			printf("[Server Error]: Cannot find requested file.\n");
	}else if(s_code == '1'){
    int file_size, block_size = MAX_BLOCK_SIZE;
    int nr, nw;

    printf("[Client]: Ready to accept file\n");
    // read the file size
		readn(sd, (char *)&file_size,4);
    printf("[Client]: File size: %d\n", file_size);

    // indicate the block_size to transfer data into file
    if (block_size > file_size)
      block_size = file_size;
		char block[block_size];

    // copy data into file
    fd = open(filename,O_WRONLY | O_CREAT, 0766);
    while(file_size > 0)
    {
        // indicate the block size to transfer data
        block_size = (block_size > file_size) ? file_size : block_size;
        // read data from socket buffer into block of data
        nr = readn(sd, block, block_size);
        // write from block to file
        if((nw = writen(fd, block, nr)) < nr)
        {
            printf("[Client Error]: Write to file error\n");
            close(fd);
            return;
        }
        // decrease the size of file
        file_size -= nw;
      }
		if(file_size==0){
			printf("[Client]: File received.\n");
		}
    memset(block, 0, block_size);
		close(fd);
    fflush(stdout);
     }
  else{
       printf("S_CODE: %c\n", s_code);
     };
  }
  else{
    printf("[Client Error]: Duplicated file name\n");
    close(fd);
  }
}

/* handler command pwd */
void handler_pwd(int sd){
  char c_code = 'C';
  int length, nw;

  //Send pwd code to server
  if(writen(sd, &c_code, 1) < 0){
    printf("[Client Error]: Cannot send command to server\n");
    return;
  };

  fflush(stdout);
  // read the size of current working directory path of server
  readn(sd,(char *)&length,2);
  length = (int) ntohs(length);

  // read the current working directory path of server
  char path[length];
  readn(sd,path,length);

  printf("[Server]: Current Directory- %s\n", path);
}

/* handler command dir */
void handler_dir(int sd){
  char c_code = 'D';
  char s_code;
  int length;

   //Send dir code to server
  writen(sd, &c_code, 1) ;
  //Read code sent by server
  readn(sd, &s_code, 1);
  if(s_code == '0'){
    fflush(stdout);
    printf("[Server Error]: Command dir failed.\n");
  }else if(s_code == '1'){
    fflush(stdout);
    //Read the size of send back buffer
    readn(sd,(char *)&length,4);
    char dir_list[length];

    // read the directory list
    readn(sd,dir_list,length);
    dir_list[length]='\0';

    printf("%s\n",dir_list);
  }else {
    printf("[Server]: Wrong code %c.\n",s_code);
  }
}

/* handle cd command */
void handler_cd(int sd, char path[]){
  int pathlen = strlen(path);
  char c_code = 'E', s_code;

  // send cd code to server
  if(writen(sd, &c_code, 1) < 0){
    printf("[Client Error]: Cannot send command to server\n");
    return;
  };

  // send the size of the path
  writen(sd,(char *)&pathlen, 2);
  fflush(stdout);
  // send the path
  writen(sd,path,pathlen);

  // read server feedback code
  readn(sd, &s_code, 1);
  if(s_code == '1'){
    printf("[Server]: Directory has been changed successfully!\n");
  }else if(s_code == '0'){
      printf("[Server]: Change directory failed.\n");
  }else{
    printf("[Server]: Wrong code.\n");
  }
  fflush(stdout);
}

/* handle command ldir*/
void handler_ldir(){
  DIR *dir;
  struct dirent *d;

  // open current directory in client side
  dir = opendir(".");
  if(dir!=NULL){
    while((d = readdir(dir))!= NULL){
      printf("%s\n",d->d_name);
    }
    closedir(dir);
  }else{
    printf("[Client]: Commnad ldir failed!\n");
  }
}

/* handler command lpwd */
void handler_lpwd(){
  char dir[MAX_BLOCK_SIZE];

  getcwd(dir,MAX_BLOCK_SIZE);
  printf("[Client]: Current directory- %s\n",dir);
}

/* handle command lcd */
void handler_lcd(char path[]){
  int condition=chdir(path);
  if(condition==0){
    printf("[Client]: Directory successfully changed.\n");
  }else{
    printf("[Client]: Directory changing fail.\n");
  }
}
