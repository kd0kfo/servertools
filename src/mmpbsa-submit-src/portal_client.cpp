#include "globals.h"

#include <cstdlib>
#include <cstdio>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cerrno>
#include <limits.h>

void send_message(int socket_fd,const char *message)
{
  const size_t buffer_size = 4096;
  char *buffer = NULL;
  
  ssize_t chars_read;

  buffer = (char*)calloc(buffer_size,sizeof(char));
  if(buffer == NULL)
    {
      perror("Could not allocate memory for receiving buffer.");
      exit(errno);
    }
    
  write(socket_fd,message,strlen(message)+1);// +1 for the null character
  
  while(true)
    {
      chars_read = read(socket_fd,buffer,buffer_size);
      if(chars_read == 0)
	return;
      if(chars_read < 0)
	{
	  perror("ERROR - Problem reading socket");
	  exit(errno);
	}
      fwrite(buffer,sizeof(char),chars_read,stdout);
    }
}

bool sanity_check_path(const char *path)
{
  using std::string;

  string dir;
  struct stat info;
  if(path == NULL)
    return false;
  
  dir = path;
  if(dir.find_last_of("/") != string::npos)
    dir = dir.erase(dir.find_last_of("/"));
  if(dir.size() == 0)
    dir = "./";
  
  if(stat(dir.c_str(),&info) != 0)
    {
      fprintf(stderr,"ERROR - Error in obtaining file info on directory (%s)\n",dir.c_str());
      fprintf(stderr,"Reason:\n%s\n",strerror(errno));
      exit(errno);
    }
  
  if(info.st_gid != GRID_USERS_GID)
    return false;
  
  if((info.st_mode & S_IWGRP) == 0)
    return false;
  
  return true;
}

int main(int argc, const char **argv)
{
  
  int socket_fd;

  const size_t path_buffer_size = PATH_MAX+16;
  char *file_path_buffer = NULL;

  struct sockaddr_in name;
  struct hostent* hostinfo;

  socket_fd = socket(PF_INET,SOCK_STREAM,0);
  
  name.sin_family = AF_INET;
  
  if(argc < 3)
    {
      fprintf(stderr,"ERROR - Need the hostname and script path\n");
      exit(-1);
    }

  if(!sanity_check_path(argv[2]))
    {
      fprintf(stderr,"ERR0R - Invalid submission script path\n");
      fprintf(stderr,"Note: Directory must be writable by group %u\n",GRID_USERS_GID);
      exit(-1);
    }

  hostinfo = gethostbyname(argv[1]);
  if(hostinfo == NULL)
    {
      fprintf(stderr,"ERROR - Error resolving hostname (%s)\n",argv[1]);
      return h_errno;
    }

  // setup server ip & port
  name.sin_addr = *((struct in_addr*) hostinfo->h_addr);
  name.sin_port = htons(1234);
  
  // Connect
  if(connect(socket_fd, (struct sockaddr*)(&name), sizeof(struct sockaddr_in)) == -1)
    {
      fprintf(stderr,"ERROR - Error connecting to %s\n",argv[1]);
      exit(-1);
    }

  file_path_buffer = (char*)calloc(path_buffer_size,sizeof(char));
  if(file_path_buffer == NULL)
    {
      perror("ERROR - Could not allocate buffer.");
      exit(errno);
    }
  if(snprintf(file_path_buffer,(path_buffer_size-1)*sizeof(char),"%lu:%s",getuid(),argv[2]) == 0)
    {
      fprintf(stderr,"ERROR - Could not create id/path message for grid server.\n");
      exit(-1);
    }
  
  send_message(socket_fd,file_path_buffer);
  
  free(file_path_buffer);
  return 0;
}
