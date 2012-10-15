#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <getopt.h>
#include <dirent.h>

#include "submit_job.h"
#include "grid_master.h"

class GridException;

#define DEFAULT_PORT "1234"// Connect to this port
#define PORT_STR_MAX 16// buffer size
#define DEFAULT_BACKLOG 10// Number of Connections

enum VERBOSITY{QUIET=0,WARNING,INFO};
int portal_verbosity = QUIET;

void send_msg(int comm_fd, const char *message)
{
  if(send(comm_fd, message, strlen(message)*sizeof(char), 0) == -1)
    perror("send");
}

void sigchld_handler(int s)
{
  while(waitpid(-1, NULL, WNOHANG) > 0)
    continue;
}

void* get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) 
    {
      return &(((struct sockaddr_in*)sa)->sin_addr);
    }

  return (void*)&(((struct sockaddr_in6*)sa)->sin6_addr);
}

void do_submission(int client_fd, const char *job_file)
{

  int retval;

  struct submit_job_arguments sj_args;
  struct grid_master_arguments gm_args;

  // Setup parameters
  sj_args.job_file = job_file;
  sj_args.verbose_level = 0;
  sj_args.previous_process = -1;
  sj_args.submitter_uid = -1;
  sj_args.simulate_submission = false;
  sj_args.natoms = sj_args.ifbox = -1;
  sj_args.extract_trajectory = false;

  // Setup more parameters
  gm_args.command = "start";
  gm_args.grid_master_verbosity = sj_args.verbose_level;
  gm_args.simulate_spawn = sj_args.simulate_submission;
  gm_args.processID = -1;
  gm_args.initialization = true;

  // Perform submission procedure
  retval = submit_and_start(sj_args,gm_args);
  
  if(retval == 0)
    send_msg(client_fd, "Submission succeded.\n");
  else
    send_msg(client_fd, "Submission failed.\n");
}

void parse_submission(int conn_fd, const char *uid_path, size_t strlen)
{
  using std::string;
  
  int submitter_uid = getuid();
  string new_pwd = "";
  size_t end_of_pwd;
  const char *physical_path = uid_path;
  const char *delim = NULL;
    
  if(uid_path == NULL || strlen == 0)
    {
      if(portal_verbosity > QUIET)
	fprintf(stderr,"WARNING - submission script path has zero length");
      
      return;
    }
  
  // Is the uid provided in the path string?
  if((delim = strchr(uid_path,':')) != NULL)
    {
      // If there is a uid, extract it
      if(sscanf(uid_path,"%u:",&submitter_uid) == 1)
	{
	  physical_path = delim + 1;
	}
      else
	{
	  if(portal_verbosity >= WARNING)
	    fprintf(stderr,"Invalid uid/path combination (%s)\n",uid_path);
	  send_msg(conn_fd,"Invalid uid/path combination");
	  return;
	}
    }
  
  if(access(physical_path,R_OK) != 0)
    {
      send_msg(conn_fd,physical_path);
      send_msg(conn_fd," is not a path with reading permission.\nReason: ");
      send_msg(conn_fd,strerror(errno));
      return;
    }
  
  new_pwd = physical_path;
  if((end_of_pwd = new_pwd.find_last_of('/')) != string::npos)
    {
      new_pwd = new_pwd.erase(end_of_pwd);
      if(new_pwd.size() == 0)
	new_pwd = "/";
    }
  
  if(chdir(new_pwd.c_str()) != 0)
    {
      perror("chdir");
      send_msg(conn_fd,"Could not change working directory to ");
      send_msg(conn_fd,new_pwd.c_str());
      return;
    }
  
  if(portal_verbosity >= INFO)
    printf("Running submission script (%s) as UID = %d and queue with UID = %d in directory (%s)\n",physical_path,getuid(),submitter_uid,new_pwd.c_str());
  
  try
    {
      do_submission(conn_fd,physical_path);
    }
  catch(GridException ge)
    {
      send_msg(conn_fd,"Job submission failed\nMessage:\n");
      send_msg(conn_fd,ge.what());
    }
}

struct option long_opts[] = {
  {"port",1,NULL,'p'},
  {"connections",1,NULL,'c'},
  {"debug",0,NULL,'d'},
  {"verbose",0,NULL,'v'},
  {NULL,0,NULL,0}
};
static const char short_opts[] = "c:d:p:v";

int main(int argc, char **argv)
{
  int sockfd/* listener */, new_fd/* current connection */;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // Client's address
  socklen_t sin_size;
  struct sigaction sa;
  char s[INET6_ADDRSTRLEN];
  int rv;
  int requested_opt = 1;
  int opt_flag = -1;
  int backlog = DEFAULT_BACKLOG;
  char port[PORT_STR_MAX];

  strcpy(port,DEFAULT_PORT);
  
  while((opt_flag = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1)
    {
      switch(opt_flag)
	{
	case 'c':
	  if(strcmp(optarg,":") == 0)
	    {
	      perror("ERROR - The number of connections flag requires an integer value.\n");
	      exit(-1);
	    }
	  if(scanf(optarg,"%u",&backlog) != 1)
	    {
	      fprintf(stderr,"ERROR - Could not get number of connections from parameter (%s)\n",optarg);
	      exit(-1);
	    }
	  break;
	case 'p':
	   if(strcmp(optarg,":") == 0)
	    {
	      fprintf(stderr,"ERROR - The port flag requires an integer value.\n");
	      exit(-1);
	    }
	   strncpy(port,optarg,(PORT_STR_MAX-1)*sizeof(char));
	  break;
	case 'd':
	  portal_verbosity = INFO;
	  break;
	case 'v':
	  portal_verbosity++;
	  break;
	default:
	  fprintf(stderr,"ERROR - Unknown flag (%c)",opt_flag);
	  exit(-1);
	  break;
	}// end of switch
    }// end of getopts while loop

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) 
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(-1);
    }


  // Bind Connection
  for(p = servinfo; p != NULL; p = p->ai_next) 
    {
      if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
	{
	  perror("server: socket");
	  continue;
	}

      if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &requested_opt, sizeof(int)) == -1) 
	{
	  perror("setsockopt");
	  exit(-1);
	}

      if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
	{
	  close(sockfd);
	  perror("server: bind");
	  continue;
	}
    
      break;
    }

  if(p == NULL)  
    {
      fprintf(stderr, "server: failed to bind\n");
      exit(-1);
    }

  freeaddrinfo(servinfo); // Free serverinfo memory; it is not needed anymore

  // Setup listener.
  if(listen(sockfd, backlog) == -1)
    {
      perror("listen");
      exit(-1);
    }

  // Zombies bad
  sa.sa_handler = sigchld_handler; 
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if(sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
      perror("sigaction");
      exit(1);
    }
  
  if(portal_verbosity >= INFO)
    printf("server: waiting for connections...\n");

  // Infinite loop for connections
  while(1)
    {  
      sin_size = sizeof(their_addr);
      new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
      if(new_fd == -1) 
	{
	  perror("accept");
	  continue;
	}

      inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr)
		,s, sizeof s);
      if(portal_verbosity >= INFO)
	printf("server: got connection from %s\n", s);

      if(!fork())
	{ 
	  // Connection communicator
	  size_t recv_buffer_size = 1024;
	  char recv_buffer[recv_buffer_size];

	  strcpy(recv_buffer,"EMPTY");
	  if(recv(new_fd,recv_buffer,(recv_buffer_size-1)*sizeof(char),0) < 0)
	    {
	      perror("recv");
	      send_msg(new_fd,"ERROR - Could not receive message");
	    }
	  else
	    parse_submission(new_fd,recv_buffer,recv_buffer_size);

	  close(sockfd); 
	  close(new_fd);
	  exit(0);
	}
      close(new_fd);  // parent doesn't need this
    }

  return 0;
}

