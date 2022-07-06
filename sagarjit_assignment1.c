/**
 * @sagarjit_assignment1
 * @author  Sagar Jitendra Thacker <sagarjit@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
// #include <strings.h>
// #include <sys/un.h>
// #include <ifaddrs.h>
// #include <errno.h>
#include <ctype.h>
#include <stdbool.h>

#include "../include/global.h"
#include "../include/logger.h"

#define MSG_SIZE 256
#define CMD_SIZE 100
#define IP_LEN 32
#define PORT_SIZE 6
#define HOST_SIZE 100
#define STDIN 0
#define NUM_HOST 5
#define MAXDATASIZE 256

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

void start_server(char *port_no);
void start_client(char *port_no);
int binding_socket(char *port_no);
char * get_port_number();
char * get_IP_address();
int is_valid_port(char *port_value_check);
int is_valid_IP(char *ip);
int connect_to_host(char *server_ip, char *server_port);
void *get_in_addr(struct sockaddr *sa);
char * convert_port_char(int port);
void sort_list();
char * get_server_port_number();
void sort_blocked_list();

int client_socketfd;
int server_socketfd;

struct host{
	char* hostname;
	char* ip_addr;
	char* port_num;
	int num_msg_sent;
	int num_msg_rcv;
	char* status;
	int fd;
	char blocked[NUM_HOST][IP_LEN];
	// struct host * next_host;
	bool is_logged_in;
	bool is_server;
	struct message * queued_messages;
}*host_ptr[NUM_HOST];

struct message {
char text[MAXDATASIZE];
char msg[MAXDATASIZE];
char from_ip_addr[IP_LEN];
char from_port_no[PORT_SIZE];
char to_ip_addr[IP_LEN];
char to_port_no[PORT_SIZE];
// struct host * from_client;
struct message * next_message;
bool is_broadcast;
};

struct details{
	int fd;
	char hostname[HOST_SIZE];
	char ip_addr[IP_LEN];
	char port_no[PORT_SIZE];
	bool is_logged_in;
	char blocked[NUM_HOST][IP_LEN];
};

struct blocked_list{
	char hostname[HOST_SIZE];
	char ip_addr[IP_LEN];
	char port_no[PORT_SIZE];
}blocked_list_server[5];

int main(int argc, char **argv){
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	if (argc == 3){
		if (*argv[1] == 's'){
			start_server(argv[2]);
		}
		else if (*argv[1] == 'c'){
			start_client(argv[2]);
		}
		else{
			// printf("Enter valid arguments i.e., client(c)/server(s) and PORT number\n");
			exit(1);
		}
	}
	else{
		// printf("Invalid argument values.\n");
		exit(1);
	}
	
	return 0;
}

void start_server(char *port_no){
	int BACKLOG=5;
	int fdmax, i, j;
	int newfd; // newly accept()ed socket descriptor
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	struct sockaddr_in remoteaddr, server_addr; // client address
	char remoteIP[INET6_ADDRSTRLEN];
    socklen_t addrlen, server_addrlen;
	char * server_ip_addr;
	char * server_port_number;
	int n_host = 0;
	struct message recv_msg;
	struct details server_details[NUM_HOST];
	int num_queued_msg = 0;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	server_socketfd = binding_socket(port_no);

	// Listen
	if (listen(server_socketfd, BACKLOG) < 0){
		perror("Listen Failed.\n");
	}
	else{
		// printf("Listening to socket.\n");
	}

	// Initializing host struct
	for (n_host=0; n_host<NUM_HOST; n_host++){
		host_ptr[n_host] = (struct host *)malloc(sizeof(struct host));
		host_ptr[n_host]->fd = -1;
		host_ptr[n_host]->queued_messages = (struct message *)malloc(sizeof(struct message));
		strcpy(host_ptr[n_host]->queued_messages->text, "");
		for (int n_block=0; n_block<NUM_HOST; n_block++){
			memset(&host_ptr[n_host]->blocked[n_block], '\0', sizeof(host_ptr[n_host]->blocked[n_block]));
		}
		// host_ptr[n_host]->queued_messages = (struct message *)malloc(sizeof(struct message));
		// memset(&host_ptr[n_host]->queued_messages, '\0', sizeof(host_ptr[n_host]->queued_messages));
	}

	FD_SET(STDIN, &master); // Add STDIN in fd set
	FD_SET(server_socketfd, &master); // Add the server socket fd in fd set
	
	// keep track of the biggest file descriptor
    fdmax = server_socketfd; // so far, it's this one
	
	for(;;){
		fflush(stdout);
		read_fds = master;
		
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("Select");
			exit(4);
		}
		
		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++){
			if (FD_ISSET(i, &read_fds)){
				if (i == STDIN){
					char *command = (char*) malloc(sizeof(char)*CMD_SIZE);
					if(fgets(command, CMD_SIZE-1, stdin) == NULL){
							exit(-1);
					}
					int len = strlen(command);
					command[len-1] = '\0';

					if(strcmp(command, "AUTHOR") == 0){ // AUTHOR
						cse4589_print_and_log("[%s:SUCCESS]\n", command);
						cse4589_print_and_log("I, sagarjit, have read and understood the course academic integrity policy.\n");
						cse4589_print_and_log("[%s:END]\n", command);
					}
					else if(strcmp(command, "IP") == 0){ // IP
						server_ip_addr = get_IP_address();
						if (strcmp(server_ip_addr, "NULL") != 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", "IP");
							cse4589_print_and_log("IP:%s\n", server_ip_addr);
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "IP");
						}
						cse4589_print_and_log("[%s:END]\n", "IP");
					}
					else if(strcmp(command, "LIST") == 0){ // LIST
						sort_list();
						cse4589_print_and_log("[%s:SUCCESS]\n", "LIST");
						int counter = 1;
						for (int p=0; p<NUM_HOST; p++){
							// printf("%s\n", host_ptr[p]->ip_addr);
							if(host_ptr[p]->fd != -1 && host_ptr[p]->is_logged_in){
								// printf("INSIDE if\n");
								cse4589_print_and_log("%-5d%-35s%-20s%-8s\n", counter++, host_ptr[p]->hostname, host_ptr[p]->ip_addr, host_ptr[p]->port_num);
							}
						}
						cse4589_print_and_log("[%s:END]\n", "LIST");
					}
					else if(strcmp(command, "PORT") == 0){ // PORT
						server_port_number = get_server_port_number();
						if (strcmp(server_port_number, "NULL") != 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", "PORT");
							cse4589_print_and_log("PORT:%s\n", server_port_number);
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "PORT");
						}
						cse4589_print_and_log("[%s:END]\n", "PORT");
					}
					else if (strcmp(command, "STATISTICS") == 0){
						sort_list();
						int counter = 1;
						cse4589_print_and_log("[%s:SUCCESS]\n", "STATISTICS");
						for (int p=0; p<NUM_HOST; p++){
							// printf("%s\n", host_ptr[p]->ip_addr);
							if(host_ptr[p]->fd != -1){
								// printf("INSIDE if\n");
								cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", p+1, host_ptr[p]->hostname, host_ptr[p]->num_msg_sent, host_ptr[p]->num_msg_rcv, host_ptr[p]->status);
							}
						}
						cse4589_print_and_log("[%s:END]\n", "STATISTICS");
					}
					else if (strncmp(command, "BLOCKED", 7) == 0){ // BLOCKED
						char ip_addr[IP_LEN];
						int index=8, j=0;
						bool ip_present=false;

						// Extract the IP Address from the command
						while (command[index] != '\0'){
							ip_addr[j] = command[index];
							j=j+1;
							index=index+1;
						}
						ip_addr[j] = '\0';

						if (is_valid_IP(ip_addr)){
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(ip_addr, host_ptr[n_host]->ip_addr) == 0){
										ip_present = true;
									}
								}
							}

							if(ip_present){
								for(int n_host=0; n_host<NUM_HOST; n_host++){ // Initialize blocked list as null
									strcpy(blocked_list_server[n_host].hostname, "");
									strcpy(blocked_list_server[n_host].ip_addr, "");
									strcpy(blocked_list_server[n_host].port_no, "");
								}

								// printf("before find\n");
								for(int n_host=0; n_host<NUM_HOST; n_host++){ // Find all the IP address that are blocked
									if (host_ptr[n_host]->fd != -1){
										if (strcmp(ip_addr, host_ptr[n_host]->ip_addr) == 0){
											for(int n_block=0; n_block<NUM_HOST; n_block++){
												if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
													strcpy(blocked_list_server[n_block].ip_addr, host_ptr[n_host]->blocked[n_block]);
												}
											}
											break;
										}
									}
								}

								// printf("before get\n");
								for(int n_host=0; n_host<NUM_HOST; n_host++){ // Get the hostname and Port number of the blocked clients
									if (host_ptr[n_host]->fd != -1){
										for(int n_block=0; n_block<NUM_HOST; n_block++){
											if (strcmp(host_ptr[n_host]->ip_addr, blocked_list_server[n_block].ip_addr) == 0){
												strcpy(blocked_list_server[n_block].hostname, host_ptr[n_host]->hostname);
												strcpy(blocked_list_server[n_block].port_no, host_ptr[n_host]->port_num);
											}
										}
									}
								}

								// printf("before print\n");
								// Print Blocked list
								sort_blocked_list();
								cse4589_print_and_log("[%s:SUCCESS]\n", "BLOCKED");
								int counter = 1;
								for(int n_block=0; n_block<NUM_HOST; n_block++){
									if (strlen(blocked_list_server[n_block].ip_addr) != 0){
										cse4589_print_and_log("%-5d%-30s%-20s%-20s\n", counter++, blocked_list_server[n_block].hostname, blocked_list_server[n_block].ip_addr, blocked_list_server[n_block].port_no);
									}
								}
								cse4589_print_and_log("[%s:END]\n", "BLOCKED");
							}
							else{
								cse4589_print_and_log("[%s:ERROR]\n", "BLOCKED");
                            	cse4589_print_and_log("[%s:END]\n", "BLOCKED");
							}
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "BLOCKED");
                            cse4589_print_and_log("[%s:END]\n", "BLOCKED");
						}
					} // END BLOCKED
				} // END of handling STDIN commands
				else if (i == server_socketfd){
					// handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(server_socketfd, (struct sockaddr *)&remoteaddr, &addrlen);
					if (newfd == -1) {
                        perror("Accept Error");
                    } 
					else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) { // keep track of the max
                            fdmax = newfd;
                        }
						
						char host[100];
	                    getnameinfo((struct sockaddr *)&remoteaddr, addrlen, host, sizeof(host), 0,0,0);
						
						// Extract IP address
						inet_ntop(AF_INET, &remoteaddr.sin_addr.s_addr, remoteIP, INET6_ADDRSTRLEN);

						n_host = 0;
						int client_present = 0;
						while (host_ptr[n_host]->fd != -1){
							if (strcmp(host_ptr[n_host]->ip_addr, remoteIP) == 0){
								client_present = 1;
								host_ptr[n_host]->is_logged_in = true;
								strcpy(host_ptr[n_host]->status, "logged-in");
							}
							n_host ++;
						}
						
						if (client_present == 0){
							// Adding new client to host struct
							host_ptr[n_host]->hostname = malloc(HOST_SIZE); // Hostname
							strcpy(host_ptr[n_host]->hostname, host);
							
							host_ptr[n_host]->ip_addr = malloc(IP_LEN); // IP address
							strcpy(host_ptr[n_host]->ip_addr, remoteIP);

							host_ptr[n_host]->port_num = malloc(PORT_SIZE); // Port Number

							host_ptr[n_host]->num_msg_sent = 0;
							host_ptr[n_host]->num_msg_rcv = 0;
							host_ptr[n_host]->status = malloc(20); // Allocate space for status
							strcpy(host_ptr[n_host]->status, "logged-in");
							host_ptr[n_host]->fd = newfd; // FD
							host_ptr[n_host]->is_logged_in = true;
							host_ptr[n_host]->is_server = false;
							host_ptr[n_host]->queued_messages = (struct message *)malloc(sizeof(struct message));
						}
						
						// for (int m=0; m<NUM_HOST; m++){
						// 	host_ptr[n_host]->blocked[m] = (struct host *)malloc(sizeof(struct host));
						// }

						// printf("%d\n", n_host);
						// printf("%s\n", host_ptr[n_host]->hostname);
						// printf("%s\n", host_ptr[n_host]->ip_addr);
						// printf("%s\n", host_ptr[n_host]->port_num);
                    }
				} // END got new incoming connection
				else{
					memset(&recv_msg, '\0', sizeof(recv_msg));

					if (recv(i, &recv_msg, sizeof(recv_msg), 0) <= 0){
						// pass
						// printf("Inside recv\n");
					}
					else{
						if (strcmp(recv_msg.text, "SAVE_CLIENT_PORT") == 0){
							for(int a=0; a<NUM_HOST; a++){
								if (strcmp(host_ptr[a]->ip_addr, recv_msg.from_ip_addr) == 0){
									strcpy(host_ptr[a]->port_num, recv_msg.from_port_no);
									break;
								}
							}
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								server_details[n_host].fd = -1;
								for (int n_block=0; n_block<NUM_HOST; n_block++){
									strcpy(server_details[n_host].blocked[n_block], "");
								}
							}
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1 && host_ptr[n_host]->is_logged_in){
									server_details[n_host].fd = host_ptr[n_host]->fd;
									strcpy(server_details[n_host].hostname, host_ptr[n_host]->hostname);
									strcpy(server_details[n_host].ip_addr, host_ptr[n_host]->ip_addr);
									strcpy(server_details[n_host].port_no, host_ptr[n_host]->port_num);
									server_details[n_host].is_logged_in = host_ptr[n_host]->is_logged_in;
									int counter = 0;
									for (int n_block=0; n_block<NUM_HOST; n_block++){
										if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
											strcpy(server_details[n_host].blocked[counter++], host_ptr[n_host]->blocked[n_block]);
										}
									}
								}
							}
							int sent_info = send(i, &server_details, sizeof(server_details), 0);
						} // END SAVE_CLIENT_PORT
						else if (strcmp(recv_msg.text, "REFRESH") == 0){
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								server_details[n_host].fd = -1;
								for (int n_block=0; n_block<NUM_HOST; n_block++){
									strcpy(server_details[n_host].blocked[n_block], "");
								}
							}
							// printf("INSIDE REFRESH\n");
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								// printf("INSIDE FOR\n");
								// printf("%d\n", host_ptr[n_host]->fd);
								if (host_ptr[n_host]->fd != -1 && host_ptr[n_host]->is_logged_in){
									// printf("Inside IF\n");
									server_details[n_host].fd = host_ptr[n_host]->fd;
									strcpy(server_details[n_host].hostname, host_ptr[n_host]->hostname);
									strcpy(server_details[n_host].ip_addr, host_ptr[n_host]->ip_addr);
									strcpy(server_details[n_host].port_no, host_ptr[n_host]->port_num);
									server_details[n_host].is_logged_in = host_ptr[n_host]->is_logged_in;
									int counter = 0;
									for (int n_block=0; n_block<NUM_HOST; n_block++){
										if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
											strcpy(server_details[n_host].blocked[counter++], host_ptr[n_host]->blocked[n_block]);
										}
									}
								}
							}
							int sent_info = send(i, &server_details, sizeof(server_details), 0);
						} // END REFRESH
						else if (strcmp(recv_msg.text, "SEND_MESSAGE") == 0){
							int to_sock_df;
							bool client_blocked = false;
							bool client_loggedoff = false;
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(recv_msg.to_ip_addr, host_ptr[n_host]->ip_addr) == 0){
										if (host_ptr[n_host]->is_logged_in == false){ // Check if client is logged in or not
											client_loggedoff = true;
										}
										for(int n_block=0; n_block<NUM_HOST; n_block++){ // Check if client is blocked or not
											if (strcmp(recv_msg.from_ip_addr, host_ptr[n_host]->blocked[n_block]) == 0){
												client_blocked = true;
												break;
											}
										}
										if((client_blocked == false) && (client_loggedoff == false)){ // If not blocked and not logged in DONT SEND
											to_sock_df = host_ptr[n_host]->fd;
											host_ptr[n_host]->num_msg_rcv += 1;
										}
										if(client_loggedoff){ // Queue message if client logged off
											struct message new_msg;
											// memset(&new_msg, '\0', sizeof(new_msg));
											// strcpy(new_msg.text, recv_msg.text);
											// strcpy(new_msg.msg, recv_msg.msg);
											// strcpy(new_msg.from_ip_addr, recv_msg.from_ip_addr);
											// strcpy(new_msg.from_port_no, recv_msg.from_port_no);
											// strcpy(new_msg.to_ip_addr, recv_msg.to_ip_addr);
											// new_msg.next_message = NULL;
											
											if (num_queued_msg == 0){
												// printf("First msg queued\n");
												strcpy(host_ptr[n_host]->queued_messages->text, recv_msg.text);
												strcpy(host_ptr[n_host]->queued_messages->msg, recv_msg.msg);
												strcpy(host_ptr[n_host]->queued_messages->from_ip_addr, recv_msg.from_ip_addr);
												strcpy(host_ptr[n_host]->queued_messages->from_port_no, recv_msg.from_port_no);
												strcpy(host_ptr[n_host]->queued_messages->to_ip_addr, recv_msg.to_ip_addr);
												host_ptr[n_host]->queued_messages->next_message = (struct message *)malloc(sizeof(struct message));
												strcpy(host_ptr[n_host]->queued_messages->next_message->text, "");
												// host_ptr[n_host]->queued_messages-> = &new_msg;
												num_queued_msg += 1;
											}
											// if (host_ptr[n_host]->queued_messages == NULL){
											// 	printf("First msg queued\n");
											// 	host_ptr[n_host]->queued_messages = &new_msg;
											// }
											else{
												// printf("%d\n", host_ptr[n_host]->queued_messages);
												// printf("second msg queued\n");
												struct message *msg_ptr;

												// if (num_queued_msg == 3){}
												msg_ptr = host_ptr[n_host]->queued_messages;
												// printf("out\n");
												// printf("%s\n", msg_ptr->text);
												// printf("%s\n", msg_ptr->msg);
												// printf("%s\n", msg_ptr->from_ip_addr);
												// printf("%s\n", msg_ptr->next_message->text);

												while(strlen(msg_ptr->next_message->text) != 0){
													// printf("in\n");
													// printf("%s\n", msg_ptr->text);
													// printf("%s\n", msg_ptr->msg);
													// printf("%s\n", msg_ptr->from_ip_addr);
													// printf("%s\n", msg_ptr->next_message->text);
													// printf("inside\n");
													// printf("%s\n", msg_ptr->next_message->text);
													// printf("%s\n", msg_ptr->next_message->msg);
													// printf("%s\n", msg_ptr->next_message->from_ip_addr);
													// printf("%s\n", msg_ptr->next_message->next_message->text);
													msg_ptr = msg_ptr->next_message;
												}
												strcpy(msg_ptr->next_message->text, recv_msg.text);
												strcpy(msg_ptr->next_message->msg, recv_msg.msg);
												strcpy(msg_ptr->next_message->from_ip_addr, recv_msg.from_ip_addr);
												strcpy(msg_ptr->next_message->from_port_no, recv_msg.from_port_no);
												strcpy(msg_ptr->next_message->to_ip_addr, recv_msg.to_ip_addr);
												msg_ptr->next_message->next_message = (struct message *)malloc(sizeof(struct message));
												strcpy(msg_ptr->next_message->next_message->text, "");
												// printf("in\n");
												// printf("%s\n", msg_ptr->next_message->text);
												// printf("%s\n", msg_ptr->next_message->msg);
												// printf("%s\n", msg_ptr->next_message->from_ip_addr);
												// printf("%s\n", msg_ptr->next_message->next_message->text);
												// msg_ptr->next_message = &new_msg;
												num_queued_msg += 1;
											}
										}
									}
									if (strcmp(recv_msg.from_ip_addr, host_ptr[n_host]->ip_addr) == 0){ // SEND Count increase
										host_ptr[n_host]->num_msg_sent += 1;
									}
								}
							}
							if ((client_blocked == false) && (client_loggedoff == false)){ // If not blocked or not logged in then don't sent
								if (send(to_sock_df, &recv_msg, sizeof(recv_msg), 0) == sizeof(recv_msg)){
									// printf("Server sent message");
									cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
									cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", recv_msg.from_ip_addr, recv_msg.to_ip_addr, recv_msg.msg);
									cse4589_print_and_log("[%s:END]\n", "RELAYED");
								}	
							}
						} // END SEND MESSAGE
						else if (strcmp(recv_msg.text, "LOGOUT") == 0){
							for (int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(host_ptr[n_host]->ip_addr, recv_msg.from_ip_addr) == 0){
										host_ptr[n_host]->is_logged_in = false;
										strcpy(host_ptr[n_host]->status, "logged-out");
									}
								}
							}
						} // END LOGOUT
						else if (strcmp(recv_msg.text, "EXIT") == 0){ // EXIT
							for(n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(host_ptr[n_host]->ip_addr, recv_msg.from_ip_addr) == 0){
										host_ptr[n_host]->fd = -1;
									}
								}
							}
						} // END EXIT
						else if (strcmp(recv_msg.text, "BROADCAST_MESSAGE") == 0){ // BROADCAST
							int to_sock_df;
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(recv_msg.from_ip_addr, host_ptr[n_host]->ip_addr) == 0){
										host_ptr[n_host]->num_msg_sent += 1;
										continue;
									}
									if (host_ptr[n_host]->is_logged_in){
										to_sock_df = host_ptr[n_host]->fd;
										host_ptr[n_host]->num_msg_rcv += 1;
										if (send(to_sock_df, &recv_msg, sizeof(recv_msg), 0) == sizeof(recv_msg)){
											// printf("Server sent message");
										}
									}	
								}
							}
							cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
							cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", recv_msg.from_ip_addr, "255.255.255.255", recv_msg.msg);
							cse4589_print_and_log("[%s:END]\n", "RELAYED");
						} // END BROADCAST
						else if (strcmp(recv_msg.text, "RELOGIN") == 0){ // RELOGIN
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								server_details[n_host].fd = -1;
								for (int n_block=0; n_block<NUM_HOST; n_block++){
									strcpy(server_details[n_host].blocked[n_block], "");
								}
							}
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1 && host_ptr[n_host]->is_logged_in){
									server_details[n_host].fd = host_ptr[n_host]->fd;
									strcpy(server_details[n_host].hostname, host_ptr[n_host]->hostname);
									strcpy(server_details[n_host].ip_addr, host_ptr[n_host]->ip_addr);
									strcpy(server_details[n_host].port_no, host_ptr[n_host]->port_num);
									int counter = 0;
									for (int n_block=0; n_block<NUM_HOST; n_block++){
										if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
											strcpy(server_details[n_host].blocked[counter++], host_ptr[n_host]->blocked[n_block]);
										}
									}
									// server_details[n_host].is_logged_in = host_ptr[n_host]->is_logged_in;
								}
							}
							int sent_info = send(i, &server_details, sizeof(server_details), 0);
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(host_ptr[n_host]->ip_addr, recv_msg.from_ip_addr) == 0){
										host_ptr[n_host]->is_logged_in = true;
										strcpy(host_ptr[n_host]->status, "logged-in");

										if (host_ptr[n_host]->queued_messages != NULL){
											struct message *relogin_msg_ptr;
											struct message relogin_msg;
											// memset(&relogin_msg, '\0', sizeof(relogin_msg));
											
											relogin_msg_ptr = host_ptr[n_host]->queued_messages;
											while (num_queued_msg != 0){
												// printf("relogin message received\n");
												strcpy(relogin_msg.text, "SEND_MESSAGE");
												// printf("%s\n", relogin_msg_ptr->msg);
												// printf("%s\n", relogin_msg_ptr->from_ip_addr);
												
												strcpy(relogin_msg.msg, relogin_msg_ptr->msg);
												strcpy(relogin_msg.from_ip_addr, relogin_msg_ptr->from_ip_addr);
												strcpy(relogin_msg.from_port_no, relogin_msg_ptr->from_port_no);
												strcpy(relogin_msg.to_ip_addr, relogin_msg_ptr->to_ip_addr);

												if(send(host_ptr[n_host]->fd, &relogin_msg, sizeof(relogin_msg), 0) == sizeof(relogin_msg)){
													// printf("Queued message success\n");
													cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
													cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", relogin_msg.from_ip_addr, relogin_msg.to_ip_addr, relogin_msg.msg);
													cse4589_print_and_log("[%s:END]\n", "RELAYED");
													host_ptr[n_host]->num_msg_rcv += 1;
												}
												relogin_msg_ptr = relogin_msg_ptr->next_message;
												num_queued_msg -= 1;
											}
											host_ptr[n_host]->queued_messages = (struct message *)malloc(sizeof(struct message));
											num_queued_msg = 0;

											struct message relogin_success;
											strcpy(relogin_success.text, "RELOGIN_SUCCESS");

											if(send(i, &relogin_success, sizeof(relogin_success), 0) == sizeof(relogin_success)){
												// printf("%s\n", "RELOGIN SUCCESS SERVER");
											}
											// cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
											// cse4589_print_and_log("[%s:END]\n", "LOGIN");
										}
									}
								}
							}
						} // END RELOGIN
						else if (strcmp(recv_msg.text, "BLOCK") == 0){ // BLOCK
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(host_ptr[n_host]->ip_addr, recv_msg.from_ip_addr) == 0){
										for (int n_block=0; n_block<NUM_HOST; n_block++){
											if (strlen(host_ptr[n_host]->blocked[n_block]) == 0){
												strcpy(host_ptr[n_host]->blocked[n_block], recv_msg.to_ip_addr);

												for(int n_host=0; n_host<NUM_HOST; n_host++){
													server_details[n_host].fd = -1;
													for (int n_block=0; n_block<NUM_HOST; n_block++){
														strcpy(server_details[n_host].blocked[n_block], "");
													}
												}

												for(int n_host=0; n_host<NUM_HOST; n_host++){
													// printf("INSIDE FOR\n");
													// printf("%d\n", host_ptr[n_host]->fd);
													if (host_ptr[n_host]->fd != -1 && host_ptr[n_host]->is_logged_in){
														// printf("Inside IF\n");
														server_details[n_host].fd = host_ptr[n_host]->fd;
														strcpy(server_details[n_host].hostname, host_ptr[n_host]->hostname);
														strcpy(server_details[n_host].ip_addr, host_ptr[n_host]->ip_addr);
														strcpy(server_details[n_host].port_no, host_ptr[n_host]->port_num);
														server_details[n_host].is_logged_in = host_ptr[n_host]->is_logged_in;
														int counter = 0;
														for (int n_block=0; n_block<NUM_HOST; n_block++){
															if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
																strcpy(server_details[n_host].blocked[counter++], host_ptr[n_host]->blocked[n_block]);
															}
														}
													}
												}
												int sent_info = send(i, &server_details, sizeof(server_details), 0);
												break;
											}
										}
									}
								}
							}
						} // END BLOCK
						else if (strcmp(recv_msg.text, "UNBLOCK") == 0){ // UNBLOCK
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (host_ptr[n_host]->fd != -1){
									if (strcmp(host_ptr[n_host]->ip_addr, recv_msg.from_ip_addr) == 0){
										for (int n_block=0; n_block<NUM_HOST; n_block++){
											if (strcmp(recv_msg.to_ip_addr, host_ptr[n_host]->blocked[n_block]) == 0){
												strcpy(host_ptr[n_host]->blocked[n_block], "");

												for(int n_host=0; n_host<NUM_HOST; n_host++){
													server_details[n_host].fd = -1;
													for (int n_block=0; n_block<NUM_HOST; n_block++){
														strcpy(server_details[n_host].blocked[n_block], "");
													}
												}

												for(int n_host=0; n_host<NUM_HOST; n_host++){
													// printf("INSIDE FOR\n");
													// printf("%d\n", host_ptr[n_host]->fd);
													if (host_ptr[n_host]->fd != -1 && host_ptr[n_host]->is_logged_in){
														// printf("Inside IF\n");
														server_details[n_host].fd = host_ptr[n_host]->fd;
														strcpy(server_details[n_host].hostname, host_ptr[n_host]->hostname);
														strcpy(server_details[n_host].ip_addr, host_ptr[n_host]->ip_addr);
														strcpy(server_details[n_host].port_no, host_ptr[n_host]->port_num);
														server_details[n_host].is_logged_in = host_ptr[n_host]->is_logged_in;
														int counter = 0;
														for (int n_block=0; n_block<NUM_HOST; n_block++){
															if (strlen(host_ptr[n_host]->blocked[n_block]) != 0){
																strcpy(server_details[n_host].blocked[counter++], host_ptr[n_host]->blocked[n_block]);
															}
														}
													}
												}
												int sent_info = send(i, &server_details, sizeof(server_details), 0);
												break;
											}
										}
									}
								}
							}
						} // END UNBLOCK
					}
				} // END send and receive
			}
		} // END looping through file descriptors
	} // END for(;;)
}

void start_client(char *port_no){
	int server, n_host;
	bool loggedin_flag=false;
	char * client_ip_addr;
	char * client_port_no;
	struct message client_msg;
	struct details client_details[NUM_HOST];
	fd_set master, read_fds;
	int fdmax;
	struct message recv_msg;
	memset(&recv_msg, '\0', sizeof(recv_msg));
	bool already_client = false;
	// int fdsocket, client_socket, client_socket_index;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	client_socketfd = binding_socket(port_no);

	FD_SET(STDIN, &master); // Add STDIN in fd set
	FD_SET(client_socketfd+1, &master); // Add the server socket fd in fd set
	
	// keep track of the biggest file descriptor
    fdmax = client_socketfd; // so far, it's this one

	// Initializing host struct
	for (int n_host=0; n_host<NUM_HOST; n_host++){
		host_ptr[n_host] = (struct host *)malloc(sizeof(struct host));
		host_ptr[n_host]->hostname = malloc(HOST_SIZE); // Hostname
		host_ptr[n_host]->ip_addr = malloc(IP_LEN); // IP address
		host_ptr[n_host]->port_num = malloc(PORT_SIZE); // Port Number
		host_ptr[n_host]->status = malloc(PORT_SIZE); // Status
		host_ptr[n_host]->fd = -1;
	}

	for(;;){
		fflush(stdout);
		read_fds = master;
		
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("Select");
			exit(4);
		}
		
		// run through the existing connections looking for data to read
		for(int i = 0; i <= fdmax; i++){
			if (FD_ISSET(i, &read_fds)){
				if (i == STDIN){
					char *command = (char*) malloc(sizeof(char)*MSG_SIZE);
					memset(command, '\0', MSG_SIZE);
					if(fgets(command, MSG_SIZE-1, stdin) == NULL){
						exit(-1);
					}
					int len = strlen(command);
					command[len-1] = '\0';

					if((strcmp(command, "AUTHOR")) == 0){ // AUTHOR
						cse4589_print_and_log("[%s:SUCCESS]\n", command);
						cse4589_print_and_log("I, sagarjit, have read and understood the course academic integrity policy.\n");
						cse4589_print_and_log("[%s:END]\n", command);
					}
					else if ((strcmp(command, "PORT")) == 0){ // PORT
						client_port_no = get_port_number();
						if (strcmp(client_port_no, "NULL") != 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("PORT:%s\n", client_port_no);
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", command);
						}
						cse4589_print_and_log("[%s:END]\n", command);
					}
					else if((strcmp(command, "IP")) == 0){ // IP
						client_ip_addr = get_IP_address();
						if (strcmp(client_ip_addr, "NULL") != 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("IP:%s\n",client_ip_addr);
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", command);
						}
						cse4589_print_and_log("[%s:END]\n", command);
					}
					else if(strcmp(command, "LIST") == 0){ // LIST
						if (loggedin_flag){
							sort_list();
							cse4589_print_and_log("[%s:SUCCESS]\n", "LIST");
							int counter = 1;
							for (int p=0; p<NUM_HOST; p++){
								if(host_ptr[p]->fd != -1 && host_ptr[p]->is_logged_in){
									cse4589_print_and_log("%-5d%-35s%-20s%-8s\n", counter++, host_ptr[p]->hostname, host_ptr[p]->ip_addr, host_ptr[p]->port_num);
								}
							}
							cse4589_print_and_log("[%s:END]\n", "LIST");
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "LIST");
							cse4589_print_and_log("[%s:END]\n", "LIST");
						}

					}
					else if (strcmp(command, "REFRESH") == 0){ // REFRESH
						if (loggedin_flag){
							strcpy(client_msg.text, "REFRESH");
							strcpy(client_msg.from_ip_addr, get_IP_address());
							strcpy(client_msg.from_port_no, get_port_number());

							if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
								// Receive response
								int recv_status = recv(server, &client_details, sizeof(client_details), 0);

								if (recv_status > 0){
									for (n_host=0; n_host<NUM_HOST; n_host++){
										host_ptr[n_host]->fd = -1;
										for (int n_block=0; n_block<NUM_HOST; n_block++){
											strcpy(host_ptr[n_host]->blocked[n_block], "");
										}
									}
									
									cse4589_print_and_log("[%s:SUCCESS]\n", "REFRESH");
									cse4589_print_and_log("[%s:END]\n", "REFRESH");

									for (n_host=0; n_host<NUM_HOST; n_host++){
										if(client_details[n_host].fd != -1){
											host_ptr[n_host]->fd = client_details[n_host].fd;
											strcpy(host_ptr[n_host]->hostname, client_details[n_host].hostname);
											strcpy(host_ptr[n_host]->ip_addr, client_details[n_host].ip_addr);
											strcpy(host_ptr[n_host]->port_num, client_details[n_host].port_no);
											host_ptr[n_host]->is_logged_in = client_details[n_host].is_logged_in;
											int counter = 0;
											for (int n_block=0; n_block<NUM_HOST; n_block++){
												if (strlen(client_details[n_host].blocked[n_block]) != 0){
													strcpy(host_ptr[n_host]->blocked[counter++], client_details[n_host].blocked[n_block]);
												}
											}
										}
									}
								}
								// else{
								// 	printf("Receive Failed\n");
								// }
							}
							// else{
							// 	printf("Sent Failed\n");
							// }
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "REFRESH");
							cse4589_print_and_log("[%s:END]\n", "REFRESH");
						}
						
					}
					else if(strncmp(command, "SEND", 4) == 0){ // SEND
						if (loggedin_flag){
							char ip_addr[IP_LEN];
							char msg[MAXDATASIZE];
							int index=5, j=0;
							int client_present = 0;

							// Extract the IP Address from the command
							while (command[index] != ' '){
								ip_addr[j] = command[index];
								j=j+1;
								index=index+1;
							}
							ip_addr[j] = '\0';

							// Extract the Message from the command
							index = index+1;
							j = 0;
							while (command[index] != '\0'){
								msg[j] = command[index];
								index = index + 1;
								j = j + 1;
							}
							msg[j] = '\0';

							// Check IP in 
							for(int n_host=0; n_host<NUM_HOST; n_host++){
								if (strcmp(ip_addr, host_ptr[n_host]->ip_addr) == 0){
									client_present = 1;
									break;
								}
							}

							int valid_ip = is_valid_IP(ip_addr);

							// printf("%d", client_present);
							if (client_present == 0 || valid_ip == 0){
								cse4589_print_and_log("[%s:ERROR]\n", "SEND");
								cse4589_print_and_log("[%s:END]\n", "SEND");
							}
							else{
								strcpy(client_msg.text, "SEND_MESSAGE");
								strcpy(client_msg.msg, msg);
								strcpy(client_msg.from_ip_addr, get_IP_address());
								strcpy(client_msg.from_port_no, get_port_number());
								strcpy(client_msg.to_ip_addr, ip_addr);

								if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
									// printf("SENT scucessful");
									cse4589_print_and_log("[%s:SUCCESS]\n", "SEND");
									cse4589_print_and_log("[%s:END]\n", "SEND");
								}
							}
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "SEND");
							cse4589_print_and_log("[%s:END]\n", "SEND");
						}
						
					} // END SEND
					else if (strcmp(command, "LOGOUT") == 0){ // LOGOUT
						if (loggedin_flag){
							loggedin_flag = false;
							strcpy(client_msg.text, "LOGOUT");
							strcpy(client_msg.from_ip_addr, get_IP_address());
							if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
								cse4589_print_and_log("[LOGOUT:SUCCESS]\n");
								cse4589_print_and_log("[LOGOUT:END]\n");
							}
						}
						else{
							cse4589_print_and_log("[LOGOUT:ERROR]\n");
							cse4589_print_and_log("[LOGOUT:END]\n");
						}
						
					} // END LOGOUT
					else if(strncmp(command, "LOGIN", 5) == 0){ // LOGIN
						char ip_addr[IP_LEN];
						char port[PORT_SIZE];
						int index=6, j=0;

						// Extract the IP Address from the command
						while (command[index] != ' '){
							ip_addr[j] = command[index];
							j=j+1;
							index=index+1;
						}
						ip_addr[j] = '\0';
						
						// Extract the Port Number from the command
						index = index+1;
						j = 0;
						while (command[index] != '\0'){
							port[j] = command[index];
							index = index + 1;
							j = j + 1;
						}
						port[j] = '\0';

						if (is_valid_IP(ip_addr) && is_valid_port(port)){

							if (already_client){
								strcpy(client_msg.text, "RELOGIN");
								strcpy(client_msg.from_ip_addr, get_IP_address());
								strcpy(client_msg.from_port_no, get_port_number());

								if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
									// Receive response
									int recv_status = recv(server, &client_details, sizeof(client_details), 0);
									if (recv_status > 0){
										// printf("Receive success\n");
										loggedin_flag = true;
										for (n_host=0; n_host<NUM_HOST; n_host++){
											for (int n_block=0; n_block<NUM_HOST; n_block++){
												strcpy(host_ptr[n_host]->blocked[n_block], "");
											}
											if(client_details[n_host].fd != -1){
												// printf("Receive IF\n");
												host_ptr[n_host]->fd = client_details[n_host].fd;
												strcpy(host_ptr[n_host]->hostname, client_details[n_host].hostname);
												strcpy(host_ptr[n_host]->ip_addr, client_details[n_host].ip_addr);
												strcpy(host_ptr[n_host]->port_num, client_details[n_host].port_no);
												strcpy(host_ptr[n_host]->status, "logged-in");
												host_ptr[n_host]->is_logged_in = true;
												int counter = 0;
												for (int n_block=0; n_block<NUM_HOST; n_block++){
													if (strlen(client_details[n_host].blocked[n_block]) != 0){
														strcpy(host_ptr[n_host]->blocked[counter++], client_details[n_host].blocked[n_block]);
													}
												}
											}
										}
										// cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
										// cse4589_print_and_log("[%s:END]\n", "LOGIN");
									}
									// else{
									// 	printf("Receive Failed\n");
									// }
								}
								// else{
								// 	printf("Sent Failed\n");
								// }
							}
							else{ // If client is performing login for the first time
								server = connect_to_host(ip_addr, port);

								if (server < 0){
									// perror("Connect Error");
									cse4589_print_and_log("[%s:ERROR]\n", "LOGIN");
									cse4589_print_and_log("[%s:END]\n", "LOGIN");
								}
								else{
									cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
									cse4589_print_and_log("[%s:END]\n", "LOGIN");

									already_client = true;
									
									FD_SET(server, &master);
									if (server > fdmax){
										fdmax=server;
									}

									// Save Port Number to server
									strcpy(client_msg.text, "SAVE_CLIENT_PORT");
									strcpy(client_msg.from_ip_addr, get_IP_address());
									strcpy(client_msg.from_port_no, get_port_number());

									if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
										// Receive response
										int recv_status = recv(server, &client_details, sizeof(client_details), 0);
										if (recv_status > 0){
											// printf("Receive success\n");
											loggedin_flag = true;
											for (n_host=0; n_host<NUM_HOST; n_host++){
												for (int n_block=0; n_block<NUM_HOST; n_block++){
													strcpy(host_ptr[n_host]->blocked[n_block], "");
												}
												if(client_details[n_host].fd != -1){
													// printf("Receive IF\n");
													host_ptr[n_host]->fd = client_details[n_host].fd;
													strcpy(host_ptr[n_host]->hostname, client_details[n_host].hostname);
													strcpy(host_ptr[n_host]->ip_addr, client_details[n_host].ip_addr);
													strcpy(host_ptr[n_host]->port_num, client_details[n_host].port_no);
													strcpy(host_ptr[n_host]->status, "logged-in");
													host_ptr[n_host]->is_logged_in = true;
													int counter = 0;
													for (int n_block=0; n_block<NUM_HOST; n_block++){
														if (strlen(client_details[n_host].blocked[n_block]) != 0){
															strcpy(host_ptr[n_host]->blocked[counter++], client_details[n_host].blocked[n_block]);
														}
													}
												}
											}
										}
										// else{
										// 	printf("Receive Failed\n");
										// }
									}
									// else{
									// 	printf("Sent Failed\n");
									// }
								}
							}
							
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "LOGIN");
							cse4589_print_and_log("[%s:END]\n", "LOGIN");
						}
					} // END LOGIN
					else if (strcmp(command, "EXIT") == 0){ //EXIT
						strcpy(client_msg.text, "EXIT");
						strcpy(client_msg.from_ip_addr, get_IP_address());
						if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
							cse4589_print_and_log("[%s:SUCCESS]\n", "EXIT");
							cse4589_print_and_log("[%s:END]\n", "EXIT");
						}
						close(server);
						exit(0);
					} // END EXIT
					else if (strncmp(command, "BROADCAST", 9) == 0){ // BROADCAST
						if (loggedin_flag){
							char broadcast_msg[MAXDATASIZE];
							int index=10, j=0;

							// Extract the Message from the command
							while (command[index] != '\0'){
								broadcast_msg[j] = command[index];
								j=j+1;
								index=index+1;
							}
							broadcast_msg[j] = '\0';

							strcpy(client_msg.text, "BROADCAST_MESSAGE");
							strcpy(client_msg.msg, broadcast_msg);
							strcpy(client_msg.from_ip_addr, get_IP_address());
							if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
								cse4589_print_and_log("[%s:SUCCESS]\n", "BROADCAST");
								cse4589_print_and_log("[%s:END]\n", "BROADCAST");
							}
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "BROADCAST");
							cse4589_print_and_log("[%s:END]\n", "BROADCAST");
						}
					} //END BROADCAST
					else if (strncmp(command, "BLOCK", 5) == 0){ // BLOCK
						if (loggedin_flag){
							char ip_addr[IP_LEN];
							char client_ip[IP_LEN];
							int index=6, j=0;

							strcpy(client_ip, get_IP_address());

							// Extract the IP Address from the command
							while (command[index] != '\0'){
								ip_addr[j] = command[index];
								j=j+1;
								index=index+1;
							}
							ip_addr[j] = '\0';

							if (is_valid_IP(ip_addr)){
								bool ip_present = false;
								bool already_blocked = false;
								
								for (n_host=0; n_host<NUM_HOST; n_host++){ // Check if ip_addr present in LIST
									if (host_ptr[n_host]->fd != -1){
										if (strcmp(host_ptr[n_host]->ip_addr, ip_addr) == 0){
											ip_present = true;
										}
									}
								}

								// printf("ip_present: %d\n", ip_present);
								// Check if ip_addr is already blocked or not (yet to implement)
								for (n_host=0; n_host<NUM_HOST; n_host++){
									if (host_ptr[n_host]->fd != -1){
										if (strcmp(host_ptr[n_host]->ip_addr, client_ip) == 0){
											for (int n_block=0; n_block<NUM_HOST; n_block++){
												if(strcmp(host_ptr[n_host]->blocked[n_block], ip_addr) == 0){
													already_blocked = true;
												}
											}
										}
									}
								}
								// printf("client ip: %s\n", client_ip);
								// printf("already blocked: %d\n", already_blocked);
								if (ip_present && !already_blocked){
									// printf("inside if\n");
									strcpy(client_msg.text, "BLOCK");
									strcpy(client_msg.from_ip_addr, get_IP_address());
									strcpy(client_msg.to_ip_addr, ip_addr);
									if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
										cse4589_print_and_log("[%s:SUCCESS]\n", "BLOCK");
										cse4589_print_and_log("[%s:END]\n", "BLOCK");

										int recv_status = recv(server, &client_details, sizeof(client_details), 0);
										if (recv_status > 0){
											// printf("Receive success\n");
											for (n_host=0; n_host<NUM_HOST; n_host++){
												for (int n_block=0; n_block<NUM_HOST; n_block++){
													strcpy(host_ptr[n_host]->blocked[n_block], "");
												}
												if(client_details[n_host].fd != -1){
													// printf("Receive IF\n");
													host_ptr[n_host]->fd = client_details[n_host].fd;
													strcpy(host_ptr[n_host]->hostname, client_details[n_host].hostname);
													strcpy(host_ptr[n_host]->ip_addr, client_details[n_host].ip_addr);
													strcpy(host_ptr[n_host]->port_num, client_details[n_host].port_no);
													strcpy(host_ptr[n_host]->status, "logged-in");
													host_ptr[n_host]->is_logged_in = true;
													int counter = 0;
													for (int n_block=0; n_block<NUM_HOST; n_block++){
														if (strlen(client_details[n_host].blocked[n_block]) != 0){
															strcpy(host_ptr[n_host]->blocked[counter++], client_details[n_host].blocked[n_block]);
														}
													}
												}
											}
										}
									}
								}
								else{
									cse4589_print_and_log("[%s:ERROR]\n", "BLOCK");
									cse4589_print_and_log("[%s:END]\n", "BLOCK");
								}
							}
							else{
								cse4589_print_and_log("[%s:ERROR]\n", "BLOCK");
								cse4589_print_and_log("[%s:END]\n", "BLOCK");
							}
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "BLOCK");
							cse4589_print_and_log("[%s:END]\n", "BLOCK");
						}
						
					} // END BLOCK
					else if (strncmp(command, "UNBLOCK", 7) == 0){ // UNBLOCK
						if (loggedin_flag){
							char ip_addr[IP_LEN];
							int index=8, j=0;

							// Extract the IP Address from the command
							while (command[index] != '\0'){
								ip_addr[j] = command[index];
								j=j+1;
								index=index+1;
							}
							ip_addr[j] = '\0';

							if (is_valid_IP(ip_addr)){
								bool ip_present = false;
								bool already_blocked = false;
								
								for (n_host=0; n_host<NUM_HOST; n_host++){ // Check if ip_addr present in LIST
									if (host_ptr[n_host]->fd != -1){
										if (strcmp(host_ptr[n_host]->ip_addr, ip_addr) == 0){
											ip_present = true;
										}
									}
								}

								// printf("ip_present: %d\n", ip_present);
								// Check if ip_addr is already unblocked or not (yet to implement)

								if (ip_present){
									// printf("inside if\n");
									strcpy(client_msg.text, "UNBLOCK");
									strcpy(client_msg.from_ip_addr, get_IP_address());
									strcpy(client_msg.to_ip_addr, ip_addr);
									if (send(server, &client_msg, sizeof(client_msg), 0) == sizeof(client_msg)){
										cse4589_print_and_log("[%s:SUCCESS]\n", "UNBLOCK");
										cse4589_print_and_log("[%s:END]\n", "UNBLOCK");	
										
										int recv_status = recv(server, &client_details, sizeof(client_details), 0);
										if (recv_status > 0){
											// printf("Receive success\n");
											for (n_host=0; n_host<NUM_HOST; n_host++){
												for (int n_block=0; n_block<NUM_HOST; n_block++){
													strcpy(host_ptr[n_host]->blocked[n_block], "");
												}
												if(client_details[n_host].fd != -1){
													// printf("Receive IF\n");
													host_ptr[n_host]->fd = client_details[n_host].fd;
													strcpy(host_ptr[n_host]->hostname, client_details[n_host].hostname);
													strcpy(host_ptr[n_host]->ip_addr, client_details[n_host].ip_addr);
													strcpy(host_ptr[n_host]->port_num, client_details[n_host].port_no);
													strcpy(host_ptr[n_host]->status, "logged-in");
													host_ptr[n_host]->is_logged_in = true;
													int counter = 0;
													for (int n_block=0; n_block<NUM_HOST; n_block++){
														if (strlen(client_details[n_host].blocked[n_block]) != 0){
															strcpy(host_ptr[n_host]->blocked[counter++], client_details[n_host].blocked[n_block]);
														}
													}
												}
											}
										}
									}
								}
								else{
									cse4589_print_and_log("[%s:ERROR]\n", "UNBLOCK");
									cse4589_print_and_log("[%s:END]\n", "UNBLOCK");
								}
							}
							else{
								cse4589_print_and_log("[%s:ERROR]\n", "UNBLOCK");
								cse4589_print_and_log("[%s:END]\n", "UNBLOCK");
							}
						}
						else{
							cse4589_print_and_log("[%s:ERROR]\n", "UNBLOCK");
							cse4589_print_and_log("[%s:END]\n", "UNBLOCK");
						}
					} // END UNBLOCK
				}
				else{
					if (recv(server, &recv_msg, sizeof(recv_msg), 0) <= 0){
						// pass
						// printf("Inside recv\n");
					}
					else{
						if (strcmp(recv_msg.text, "SEND_MESSAGE") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
							cse4589_print_and_log("msg from:%s\n[msg]:%s\n", recv_msg.from_ip_addr, recv_msg.msg);
							cse4589_print_and_log("[%s:END]\n", "RECEIVED");
						}
						else if (strcmp(recv_msg.text, "BROADCAST_MESSAGE") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
							cse4589_print_and_log("msg from:%s\n[msg]:%s\n", recv_msg.from_ip_addr, recv_msg.msg);
							cse4589_print_and_log("[%s:END]\n", "RECEIVED");
						}
						else if (strcmp(recv_msg.text, "RELOGIN_SUCCESS") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", "LOGIN");
							cse4589_print_and_log("[%s:END]\n", "LOGIN");
						}
					}
				} // END RECEIVE MESSAGE FROM SERVER
			}
		}
	}
}

char * convert_port_char(int int_port){
	char port_number[PORT_SIZE];
	int len = sprintf(port_number, "%d", int_port);
	if (len < 0){
		return "NULL";
	}
	char * port = malloc(strlen(port_number));
    port = port_number;
	// printf("%s", port);
	return port;
}

// Working
int binding_socket(char *port_no){
	int socketfd;
	struct addrinfo address, *res;

	// Fill in socket address in details
	memset(&address, 0, sizeof address);
	address.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	address.ai_socktype = SOCK_STREAM;
	address.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, port_no, &address, &res);

	// Create socket file descriptor
	socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (socketfd < 0){
		perror("Socket Failed.\n");
	}
	else{
		// printf("Socket Created!\n");
	}

	// Bind
	if (bind(socketfd, res->ai_addr, res->ai_addrlen) < 0){
		perror("Bind Failed.\n");
	}
	else{
		// printf("Bind Successful.\n");
	}
	return socketfd;
}

char * get_port_number(){
	struct sockaddr_in port_value;
	int socket_name, result;
	socklen_t length = sizeof(port_value);
	char port_number[PORT_SIZE];

	socket_name = getsockname(client_socketfd, (struct sockaddr *)&port_value, &length);
	
	result = ntohs(port_value.sin_port);
	int len = sprintf(port_number, "%d", result);
	if (len < 0){
		return "NULL";
	}
	char * port = malloc(strlen(port_number));
    port = port_number;
	return port;
}

char * get_server_port_number(){
	struct sockaddr_in port_value;
	int socket_name, result;
	socklen_t length = sizeof(port_value);
	char port_number[PORT_SIZE];

	socket_name = getsockname(server_socketfd, (struct sockaddr *)&port_value, &length);
	
	result = ntohs(port_value.sin_port);
	int len = sprintf(port_number, "%d", result);
	if (len < 0){
		return "NULL";
	}
	char * port = malloc(strlen(port_number));
    port = port_number;
	return port;
}

char * get_IP_address(){
	struct sockaddr_in server;
	struct sockaddr_in name;
	socklen_t namelength = sizeof(name);
	char buff[IP_LEN];
    int osocket = socket(AF_INET, SOCK_DGRAM, 0);
     
    if(osocket < 0){
        perror("Socket error");
    }
    
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("8.8.8.8");
    server.sin_port = htons(53);
 
    int err = connect(osocket , (const struct sockaddr*) &server , sizeof(server));
    err = getsockname(osocket, (struct sockaddr*) &name, &namelength);
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buff, IP_LEN);

	close(osocket);

    if(p == NULL){
		return "NUll";
    }
	char * ip_address = malloc(strlen(buff));
    ip_address = buff;
    return ip_address;
}

int connect_to_host(char *server_ip, char *server_port){
	struct addrinfo hints, *res;
	int sockfd;

	// first, load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(server_ip, server_port, &hints, &res) != 0){
		perror("getaddrinfo Error");
	}

	// make a socket:
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	// connect!
	int connect_result = connect(sockfd, res->ai_addr, res->ai_addrlen);
    
	return sockfd;
}

int is_valid_IP(char *ip){
	struct sockaddr_in address;
	if (inet_pton(AF_INET, ip, &address.sin_addr) > 0){
		return 1;
	}
	else{
		return 0;
	}
}

int is_valid_port(char *port_value_check){
	int length = strlen(port_value_check);
	int port_no;
	for(int i=0; i<length; i++){
		if (!isdigit(port_value_check[i])){
			return 0;
		}
	}
	port_no = atoi(port_value_check);
	if (port_no <= 0 || port_no >= 65536){
		return 0;
	}
	return 1;
}

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sort_list(){
	int i, j;
    for (i=0; i<NUM_HOST-1; i++){
        for (j=0; j<(NUM_HOST-1-i); j++){
            if((host_ptr[j]->fd != -1) && (host_ptr[j+1]->fd != -1)){
				int port1 = atoi(host_ptr[j]->port_num);
				int port2 = atoi(host_ptr[j+1]->port_num);
				if (port1 > port2){
					struct host *temp;
					temp = host_ptr[j];
					host_ptr[j] = host_ptr[j+1];
					host_ptr[j+1] = temp;
				}
			}
        }
    }
}

void sort_blocked_list(){
	int i, j;
    for (i=0; i<NUM_HOST-1; i++){
        for (j=0; j<(NUM_HOST-1-i); j++){
            if((strlen(blocked_list_server[j].ip_addr) != 0) && (strlen(blocked_list_server[j+1].ip_addr) != 0)){
				int port1 = atoi(blocked_list_server[j].port_no);
				int port2 = atoi(blocked_list_server[j+1].port_no);
				if (port1 > port2){
					struct blocked_list temp;
					temp = blocked_list_server[j];
					blocked_list_server[j] = blocked_list_server[j+1];
					blocked_list_server[j+1] = temp;
				}
			}
        }
    }
}