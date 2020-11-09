// chat.c
// Ridge Tejuco
// Sai Pappu
// COMP 429 Fall 2020

#define TRUE 1
#define FALSE 0

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main (int argc, char* argv[])
{
	// Check command line arguments for port number =================================================================
	char port_num[10] = {0};
	int loop = TRUE;
	if(argc == 1) // Check if there is no argument passed through
	{
		fprintf(stdout,"Start Chat application by executing with port number. \n");
		exit(0);
	}
	else if(argc == 2) // Grab the argument and store in char array
	{
		strcpy(port_num,argv[1]);
	}
	// Setup server info ============================================================================================
	int s;
	struct addrinfo hints,*servinfo;
	memset(&hints,0,sizeof(hints));		// clear hints
	hints.ai_family = AF_UNSPEC;		// IPV6 or IPV4
	hints.ai_socktype = SOCK_STREAM;	// TCP stream
	hints.ai_flags = AI_PASSIVE;		// Automatic

	int status = getaddrinfo(NULL,port_num,&hints,&servinfo); // Store address info of this server into servinfo 
	if(status != 0) // Error checking for status
	{
		fprintf(stderr,"getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}
	// Create socket for listening ==================================================================================
	s = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol); // S is the listening socket
	if(s == -1) // Error checking for socket
	{
		fprintf(stderr,"could not create socket.\n");
		exit(2);
	}
	fprintf(stdout,"Socket created.\n");

	// Bind socket to local port number =============================================================================
	if(bind(s,servinfo->ai_addr,servinfo->ai_addrlen) == -1)
	{
		fprintf(stderr,"could not bind socket to port number:%s\n",port_num);
		freeaddrinfo(servinfo);
		close(s);
		exit(3);
	}
	fprintf(stdout,"Bound socket to Port: %s \n", port_num);

	// Start listening to the socket ================================================================================
	if(listen(s,5) == -1)
	{
		fprintf(stderr,"server not listening \n");
		freeaddrinfo(servinfo);
		close(s);
		exit(4);
	}
	fprintf(stdout,"Listening on Port: %s \n",port_num);

	// Get host ip for later use ===================================================================================
	char hostname[40] = {0};
	struct hostent *host;
	if(gethostname(hostname,40) == -1)
	{
		fprintf(stderr,"gethostname ERROR \n");
	}
	else
	{
		host = gethostbyname(hostname);
	}

	// Setup structs and data storage for incoming connections ======================================================
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int new_fd;
	int fdmax;
	int inc;

	char remoteIP[INET6_ADDRSTRLEN] = {0};
	char buf[200] = {0};
	char peerip[20] = {0};
	char *command_name;

	fd_set master;
	fd_set read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0,&master);	//stdin
	FD_SET(s,&master);	//listening
	fdmax = s;
	int FD_ARR[20];
	int connections = 0;
	fprintf(stdout,"Starting loop. \n");

	// Client server command line interface loop ====================================================================
	while(loop) 
	{
		fprintf(stdout,"localhost:");
		fflush(stdout);
    	fflush(stdin);
		read_fds = master; // Copy master file descriptor into read_fds
		// Check which sockets are ready for handling, If none timeout after 0.25 seconds
    	select_restart:
		if(select(fdmax+1,&read_fds,NULL,NULL,NULL)==-1)
		{
		    if(errno == EINTR)
		    {
		    	goto select_restart;
		    }
			fprintf(stderr,"Select error\n");
			exit(5);
		}
		for(inc = 0;inc <= fdmax; inc++) // Start socket loop
		{
			if(FD_ISSET(inc,&read_fds)) // Loop through all open fds, anc check if its within the set
			{
				if(inc == s) // Handle the listenening socket and accept new connections ============================
				{					
					if(connections < 20)
					{
						addr_size = sizeof their_addr;
						new_fd = accept(s,(struct sockaddr *)&their_addr,&addr_size); // Accept next connection within backlog 
						if(new_fd != -1)
						{
							FD_SET(new_fd,&master); // add fd to master set
							if(new_fd > fdmax)
							{
								fdmax = new_fd; // keep track of largest fd
							}
							FD_ARR[connections] = new_fd; // add fd to fd_arr for easy listing and ID
              				connections++; // increase size of array
              				fprintf(stdout,"new connection accepted\n");
						}
					}	
				}
				else if(inc != 0) // Handle sockets =================================================================
				{
					// Handle Received data  ===============================================================================
					int msg_status = recv(inc,buf,200,0); // check inc (socket) for a msg and store it into buf
					if(msg_status <= 0) // msg status is  closed (0) or error (-1) 
					{
						if(msg_status < 0)
							fprintf(stderr,"could not read message from connection: %d. \n",inc);
						else if (msg_status == 0)
						{
							fprintf(stdout,"Connection from %d has closed. \n",inc);
						}
              				close(inc);
							FD_CLR(inc,&master);
							int jj;
							for(jj=0;jj<connections;jj++) // Iterate through array and find which element is this socket
							{
								if(FD_ARR[jj]==inc)
								{
									break;
								}
							}
							int ii; 
							for(ii = jj;ii < connections;ii++) // use jj to remove inc from FD_ARR 
							{
								if(ii != 19)
								{
									FD_ARR[ii] = FD_ARR[ii+1];
								}
							}
							connections--; // Reduce size of FD_ARR by 1
					}
					else // Print out the message ========================================================================
					{
						addr_size = sizeof their_addr;
						if(getpeername(inc,(struct sockaddr*)&their_addr,&addr_size)==-1) // Find out IP of who sent the message
							fprintf(stderr,"could not get peer name. \n");
						else
						{	
							struct sockaddr_in *sin = (struct sockaddr_in *)&their_addr; // Cast to sockaddr structure
							strcpy(peerip,inet_ntoa(sin->sin_addr));
							fprintf(stdout,"%s: %s \n",peerip,buf); // Print IP then message
						}
					}
				}
				else // handle stdin ================================================================================
				{
          			buf[0] = 0;
					fgets(buf,100,stdin);
					command_name = strtok(buf," \n");
					// Handle client commands  ============================================================
					if(strncmp(command_name,"help",20) == 0) // help command
					{
						fprintf(stdout,"============================================================\n");
						fprintf(stdout,"============================================================\n");
						fprintf(stdout,"1. help: Display list of client commands. \n");
						fprintf(stdout,"2. myip: Display the IP address of host process. \n");
						fprintf(stdout,"3. myport: Display the port of the listening process for incoming connections. \n");
						fprintf(stdout,"4. connect <destination> <port no>: Establish TCP connection to the <destination> on <port>. \n");
						fprintf(stdout,"5. list: Display a list of current connections. \n");
						fprintf(stdout,"6. terminate <id>: Close connection given <id> of 0 through 19. \n");
						fprintf(stdout,"7. send <id> <message>: Send a chat <message> to the given <id>. \n");
						fprintf(stdout,"8. exit: Close all connections and exit the program. \n");
						fprintf(stdout,"============================================================\n");
						fprintf(stdout,"============================================================\n");
					}
					else if(strncmp(command_name,"myip",20) == 0) // myip command ================================
					{
						fprintf(stdout,"MYIP: %s\n",inet_ntoa(*((struct in_addr*) host->h_addr_list[0]))); // converts IP address of network byte order to numbers and dots notation
					}
					else if(strncmp(command_name,"myport",20) == 0) // myport command
					{
						fprintf(stdout,"MYPORT: %s\n",port_num);
					}
					else if(strncmp(command_name,"connect",20) == 0) // connect command ================================
					{
						char iptoken[20]={0};
						strcpy(iptoken,strtok(NULL," \n")); // Get IP from command line token
						char port_token[5]={0};
						strcpy(port_token,strtok(NULL," \n"));	// Get port from command line token
						if(iptoken == NULL || port_token == NULL)
						{
							fprintf(stderr, "Please enter proper IP address and port number.\n");
						}
						else
						{
							fprintf(stdout,"Attempting to connect to destination: %s:%s\n",iptoken,port_token);
							if(getaddrinfo(iptoken,port_token,&hints,&servinfo) == -1) // Store socket information in servinfo
							{
								fprintf(stderr,"getaddrinfo error: %s\n", gai_strerror(status));
							}
							else
							{
								new_fd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol); // create socket
								if(new_fd == -1)
								{
									fprintf(stderr,"Could not create socket.\n");
								}
								else
								{
									if(connections < 20) // check if lt 20 connections
									{
										if(connect(new_fd,servinfo->ai_addr,servinfo->ai_addrlen)==-1) // connect through socket
										{
											fprintf(stderr, "Could not make connection.\n"); //connect error check
										}
										else
										{
											fprintf(stdout,"Connection established at %s on %s.\n",iptoken,port_token);
											FD_SET(new_fd,&master); // add socket to master fd_set
											FD_ARR[connections] = new_fd; // add socket to FD_ARR
											connections++;
											if(new_fd > fdmax)
											{
												fdmax = new_fd; // keep track of largest FD
											}
										}
									}
									else
										fprintf(stderr,"too many connections. Only 20 can be made.\n");
								}
							}
						}
					}
					else if(strncmp(command_name,"list",20) == 0) // list command ================================
					{
						fprintf(stdout,"============================================\n");
						fprintf(stdout,"id: IP address 		Port No.\n");
						int jj;
						for(jj = 0;jj < connections;jj++)
						{

							addr_size = sizeof their_addr;
							if(getpeername(FD_ARR[jj],(struct sockaddr*)&their_addr,&addr_size)==-1)
							{
								fprintf(stderr,"could not get peer name. \n");
							}
							else
							{	
								char peerip[20] = {0};
								char peerport[20] = {0};                 
								struct sockaddr_in *sin = (struct sockaddr_in*)&their_addr;
								strcpy(peerip,inet_ntoa(sin->sin_addr));                                                           
								sprintf(peerport,"%d",ntohs(sin->sin_port));  
                				fprintf(stdout,"%d: %s 	%s\n",jj,peerip,peerport);
							}
						}
						fprintf(stdout,"============================================\n");
					}
					else if(strncmp(command_name,"terminate",20) == 0) // terminate command ================================
					{
						char idtoken[5] = {0};
            			memset(idtoken,0,5);
						strcpy(idtoken,strtok(NULL," \n"));
						if(idtoken != NULL)
						{
							int id;
							sprintf(idtoken,"%d",&id);
							if(close(FD_ARR[id])!= -1)
							{
                    					fprintf(stdout,"Cleared %d \n",FD_ARR[id]);
										FD_CLR(FD_ARR[id],&master);
										int ii;
										for(ii = id;ii < connections;ii++)
										{
											if(ii != 19)
											{
												FD_ARR[ii] = FD_ARR[ii+1];
											}
										}
										connections--;
              				}
							else
								fprintf(stderr,"Could not close connection: %s\n",idtoken);
						}
						else
						{
							fprintf(stderr,"Please enter a proper id.\n");
						}
					}
					else if(strncmp(command_name,"send",20) == 0) // send command ================================
					{
						char idtoken[5] = {0};	// Grab socket id from command line token
						strcpy(idtoken,strtok(NULL," \n"));
						int id;
						sprintf(idtoken,"%d",id); // convert to integer
						fprintf(stdout,"ID: %d\n",id);
						if(id > -1 && id < 20)
						{
							char msg[200] = {0};
              				memset(msg,0,200);
							strcpy(msg,strtok(NULL,"\n")); 	// Grab the message from command line token 
							int len = strlen(msg);
							int bytes_sent = send(FD_ARR[id],msg,len,0); // send message 
							if(bytes_sent == -1)
							{
								fprintf(stderr,"Could not send message to connection: %s. Check if id is in list.\n", idtoken);		
							}
						}
						else
						{
							fprintf(stderr, "please select an id from the list\n");
						}
					}
					else if(strncmp(command_name,"exit",20) == 0) // exit command ================================
					{
						int cnt;
						for(cnt = 0; cnt < fdmax+1; cnt++) // Iterate through all FDs upto fdmax
						{
							if(FD_ISSET(cnt,&read_fds)) // check if its within our set then close it
								close(cnt);
						}
						loop = FALSE; // close loop then continue to end of program
					}
					else
					{
						fprintf(stderr,"Unknown command. Enter \"help\" for more information.\n");
					}
				}
			}
		}
	}//END While loop
	close(s);
	freeaddrinfo(servinfo);
	//free(command_name);
	return 0;
}