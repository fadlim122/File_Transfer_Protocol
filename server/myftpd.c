/*
*  myftpd.c
*	       This is improved on the base "ser6.c".
*             This is the server program for Project 2
*
*  revised:	17/11/2019
*/

#include <stdlib.h>     /* strlen(), strcmp() etc */
#include <stdio.h>      /* printf()  */
#include <string.h>     /* strlen(), strcmp() etc */
#include <errno.h>      /* extern int errno, EINTR, perror() */
#include <signal.h>     /* SIGCHLD, sigaction() */
#include <syslog.h>
#include <sys/types.h>  /* pid_t, u_long, u_short */
#include <sys/socket.h> /* struct sockaddr, socket(), etc */
#include <sys/wait.h>   /* waitpid(), WNOHAND */
#include <netinet/in.h> /* struct sockaddr_in, htons(), htonl(), */
                        /* and INADDR_ANY */
#include "stream.h"     /* MAX_BLOCK_SIZE, readn(), writen() */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>      /* O_RDONLY, O_WRONLY, O_CREAT  */
#include <time.h>
#include <dirent.h>

#define SERV_TCP_PORT   40145   /* default server listening port */
#define LOG_FILE "ServerLog.log"

/* get the current time */
char* getTimeLog(){
  time_t rawtime;
  struct tm * timeinfo;

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );

  return asctime (timeinfo);
}

  /* claim childer process */
void claim_children()
{
   pid_t pid=1;

   while (pid>0) { /* claim as many zombies as we can */
       pid = waitpid(0, (int *)0, WNOHANG);
   }
}

  /* make the server become daemon process */
void daemon_init(void)
{
   pid_t   pid;
   struct sigaction act;

   if ( (pid = fork()) < 0) {
        perror("fork"); exit(1);
   } else if (pid > 0) {
        printf("Hay, you'd better remember my PID: %d\n", pid);
        exit(0);                  /* parent goes bye-bye */
   }

   /* child continues */
   setsid();                      /* become session leader */
//     chdir("/root/assignment");                    /* change working directory */
   umask(0);                      /* clear file mode creation mask */

   /* catch SIGCHLD to remove zombies from system */
   act.sa_handler = claim_children; /* use reliable c_code */
   sigemptyset(&act.sa_mask);       /* not to block other c_codes */
   act.sa_flags   = SA_NOCLDSTOP;   /* not catch stopped children */
   sigaction(SIGCHLD,(struct sigaction *)&act,(struct sigaction *)0);
   /* note: a less than perfect method is to use
            c_code(SIGCHLD, claim_children);
   */
}

/* handle put command from client */
void put_handler(int sd, FILE *log){
  int fd;
  int file_name_size;
  char s_code = '1';

  // get the file name from client side
  readn(sd, (char *)&file_name_size, 2);
  char file_name[file_name_size+1];
  readn(sd, file_name, file_name_size);
  // adding null terminator
  file_name[file_name_size] = '\0';

  // open file, check if file exited or not
  if((fd = open(file_name,O_RDONLY)) != -1)
  {
      fprintf(log, "[%s]\t%d file already exist\n", getTimeLog(),getpid());
      s_code = '2';
  }
  else if((fd = open(file_name,O_WRONLY | O_CREAT, 0766)) == -1)
  {
      fprintf(log, "[%s]\t%d cannot create file\n", getTimeLog(),getpid());
      s_code = '0';
  }
  else
  {
      fprintf(log, "[%s]\t%d file created\n",getTimeLog(), getpid());
  }
  // send feedback code to client
  writen(sd, &s_code, 1);

  if(s_code == '1')
  {
      int file_size, block_size, nr, nw;
      // read the file size
      readn(sd, (char *)&file_size, 4);
      // indicate the block size to transfer data
      block_size = (MAX_BLOCK_SIZE > file_size) ? file_size : MAX_BLOCK_SIZE;
      char block[block_size];

      while(file_size > 0)
      {
          // indicate the block size to transfer data
          block_size = (block_size > file_size) ? file_size : block_size;
          // read data from socket buffer into block of data
          nr = readn(sd, block, block_size);
          // write from block to file
          if((nw = write(fd, block, nr)) < nr)
          {
              fprintf(log, "[%s]\t%d wrote only %d bytes out of %d\n",getTimeLog(), getpid(), nw, nr);
              return;
          }
          // decrease the size of file
          file_size -= nw;
      }
      close(fd);
      fprintf(log, "[%s]\t%d has completed put_handler\n",getTimeLog(), getpid());
  }
  else
  {
      fprintf(log, "[%s]\t%d has failed put handler\n",getTimeLog(), getpid());
  }
  fflush(log);
}

/* handler get command from client*/
void get_handler(int sd, FILE *log)
{
  int  file_size, fd;
  char s_code = '1';
  int file_name_size;

  // read the file name from client
  readn(sd, (char *)&file_name_size, 2);
  char file_name[file_name_size];
  readn(sd, file_name, file_name_size);
  file_name[file_name_size] = '\0';

   // check if file exist on server
  if((fd = open(file_name, O_RDONLY)) == -1)
  {
      fprintf(log, "[%s]\t%d file not found\n",getTimeLog(), getpid());
      s_code = 0;
      writen(sd, (char *)&s_code, 1);
      return;
  }

  // send feedback code to client
  writen(sd, &s_code, 1);

  // send the file size to client
  file_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  writen(sd, (char *)&file_size, 4);

  // store data from file to block
  char block[MAX_BLOCK_SIZE];
  int nr;

  while((nr = read(fd, block, MAX_BLOCK_SIZE)) > 0)
  {
    // write from block to buffer socket
      if(writen(sd,block, nr) == -1)
      {
          fprintf(log, "[%s]\t%d failed to send file\n",getTimeLog(), getpid());
          return;
      }
  }
  close(fd);
  fprintf(log, "[%s]\t%d completed get_handler\n",getTimeLog(), getpid());
  fflush(log);
}

/* handle pwd command */
void pwd_handler(int sd, FILE *log)
{
  int size, nr;
  char path[MAX_BLOCK_SIZE];

  // get the current directory path
  getcwd(path, sizeof(path));
  size = strlen(path);
  // send the size of the directory path to client
  writen(sd,(char *)&size, 2);
  // send the directory path to client
  writen(sd, path, size);
  fprintf(log, "[%s]\t%d, pwd_handler has completed\n",getTimeLog(), getpid());
  fflush(log);
}

/* handle dir command*/
void dir_handler(int sd, FILE *log)   // dir handler to print list of files
{
    char s_code;
    int size;
    char files[MAX_BLOCK_SIZE];
    DIR *dir;
    struct dirent *dirFiles;

    // set the array files to all 0 as reset
   memset(files,0,MAX_BLOCK_SIZE);

   // check if '.' dir is not null
    if((dir = opendir(".")) != NULL)
    {
        while((dirFiles = readdir(dir)) != NULL)
        {
            strcat(files, dirFiles->d_name);
            strcat(files, "\t");
        }

        // set null terminator
        files[strlen(files)-1] = '\0';
        s_code = '1';
        closedir(dir);
    }
    else
        s_code = '0';

    size = strlen(files);
    // if the directory is not null
    if(s_code == '1')
    {
        // send feedback code
        writen(sd, &s_code, 1);

        // send the directory list
        writen(sd,(char *) &size, 4);
        writen(sd, files, size);

        fprintf(log, "[%s]\t%d, has retrieved the list of files\n",getTimeLog(), getpid());
    }
    else
    {
        writen(sd, &s_code, 1);          // send the opcode to client
        // writen(sd, &ackcode, 1);         // send the ackcode to client
        fprintf(log, "[%s]\t%d, failed to retrieve list of files\n",getTimeLog(), getpid()); // write log
    }
    fprintf(log, "[%s]\t%d, dir_handler has completed\n",getTimeLog(), getpid());    // write log
    fflush(log);   // flush all the remaining message to log so that nothing is left behind
}

/* handler cd command from client */
void cd_handler(int sd, FILE *log)
{
  char s_code;
  int size;

  // read the path from client
  readn(sd, (char *)&size, 2);
  char path[size+1];
  readn(sd, path, size);
  // set null terminator
  path[size] = '\0';

  // change directory
  if(chdir(path) == 0){
    s_code = '1';
    fprintf(log, "[%s]\t%d, Directory changed successful\n",getTimeLog(), getpid());
  }
  else{
    s_code = '0';
    fprintf(log, "[%s]\t%d, Directory changed failed\n",getTimeLog(), getpid());
  }

  // write feedback code to client
  writen(sd, &s_code, 1);
  fprintf(log, "[%s]\t%d, cd_handler has completed\n",getTimeLog(), getpid());
  fflush(log);
}

/* serve client function to serve each connected client */
void serve_a_client(int sd, FILE *log)
{
  int nr, nw;
  char buf[MAX_BLOCK_SIZE];
  char c_code;

  while (1){

       // read the commande code from client
       if ((nr = readn(sd, &c_code, 1)) <= 0){
         /* connection broken down */
          return;
       }

       // handle command
       switch(c_code)
       {
           case 'A':
               put_handler(sd, log);
               break;
           case 'B':
               get_handler(sd, log);
               break;
           case 'C':
               pwd_handler(sd, log);
               break;
           case 'D':
               dir_handler(sd, log);
               break;
           case 'E':
               cd_handler(sd, log);
               break;
           case 'Q':
               fprintf(log, "[%s]\tClient %d has disconnect\n",getTimeLog(), getpid());
               fflush(log);
               break;
           default :
               fprintf(log, "[%s]\tInvalid opcode received\n",getTimeLog());
               fflush(log);
               break;
       }
  }
  fclose(log);
}

/* main function */
int main(int argc, char *argv[])
{
   FILE *log;
   int sd, nsd, n;
   pid_t pid, c_pid;
   unsigned short port = SERV_TCP_PORT;   // server listening port
   socklen_t cli_addrlen;
   struct sockaddr_in ser_addr, cli_addr;

   log = fopen(LOG_FILE, "w+");  // open LOG_FILE
   if(log == NULL)    // error opening
   {
       printf("Error opening LOG_FILE\n");
       exit(1);
   }
   else if(!log)    // cannot create LOG_FILE
   {
       printf("%s LOG_FILE cannot be created\n", LOG_FILE);
       exit(1);
   }

   fprintf(log, "[%s]\tServer started\n",getTimeLog());   // write to log
   fflush(log);   // flush all the message to log to ensure nothing is left behind

   /* directory */
   if(argc == 2)       // get command line argument for initial directory path
   {
       char *init_curr_dir = argv[1];
       printf("pointer path is: %s\n", init_curr_dir);
       chdir(init_curr_dir);
   }
   else if(argc > 2)     // invalid command line argument
   {
       printf("Invalid format, Format myftpd [ initial_current_directory ]\n");
       exit(1);
   }
   else
       chdir("/");    // set default initial directory

   /* turn the program into a daemon */
   daemon_init();

   /* set up listening socket sd */
   if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
         perror("server:socket"); exit(1);
   }

   /* build server Internet socket address */
   bzero((char *)&ser_addr, sizeof(ser_addr));
   ser_addr.sin_family = AF_INET;
   ser_addr.sin_port = htons(port);
   ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   /* note: accept client request sent to any one of the
      network interface(s) on this host.
   */

   /* bind server address to socket sd */
   if (bind(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr))<0){
         perror("server bind"); exit(1);
   }

   /* become a listening socket */
   listen(sd, 5);

   while (1) {

        /* wait to accept a client request for connection */
        cli_addrlen = sizeof(cli_addr);
        nsd = accept(sd, (struct sockaddr *) &cli_addr, &cli_addrlen);
        if (nsd < 0) {
             if (errno == EINTR)   /* if interrupted by SIGCHLD */
                  continue;
             perror("server:accept"); exit(1);
        }

        /* create a child process to handle this client */
        if ((pid=fork()) <0) {
            perror("fork"); exit(1);
        } else if (pid > 0) {
            c_pid = pid;
            fprintf(log, "[%s]\tClient %d has connected\n",getTimeLog(), c_pid); // write log
            fflush(log);   // flush all message to log to ensure nothing is left behind
            fclose(log);    // close log
            close(nsd);
            continue; /* parent to wait for next client */
        }

        /* now in child, serve the current client */
        close(sd);
        serve_a_client(nsd, log);
        exit(0);
   }
}
