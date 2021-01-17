s/*b05902118 陳盈如*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <time.h>

#define TIMEOUT_SEC 5       // timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024
#define NO_USE      0       // status of a http request
#define ERROR       -1  
#define READING     1       
#define WRITING     2       
#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
    char c_time_string[100];
} TimeInfo;

typedef struct {
    char hostname[512];     // hostname
    unsigned short port;    // port to listen
    int listen_fd;          // fd to wait for a new connection
} http_server;

typedef struct {
    int conn_fd;		      // fd to talk with client
    int status, pid, pid_on;  // not used, error, reading (from client), writing (to client)
    char file[MAXBUFSIZE];    // requested file
    char query[MAXBUFSIZE];   // requested query
    char host[MAXBUFSIZE];    // client host
    char *buf;                // data sent by/to client
    size_t buf_len;           // bytes used by buf
    size_t buf_size;          // bytes allocated for buf
    size_t buf_idx;           // offset for reading and writing
} http_request;

static char *logfilenameP;  // log file name


// Forwards
//
static void init_http_server( http_server *svrP,  unsigned short port );
// initailize a http_request instance, exit for error

static void init_request( http_request *reqP );
// initailize a http_request instance

static void free_request( http_request *reqP );
// free resources used by a http_request instance

static int read_header_and_file( http_request *reqP, int *errP );
// return 0: success, file is buffered in reqP->buf with reqP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: continue to it until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected

static void add_to_buf( http_request *reqP, char* str, size_t len );
static void strdecode( char* to, char* from );
static int hexit( char c );
static char* get_request_line( http_request *reqP );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );

static void set_ndelay( int fd );
// Set NDELAY mode on a socket.
int valid(char filename[128]);

int output = 0, sig = 0, count = 0;

int mmap_write(char file[])
{
    int fd,i;
    time_t current_time;
    char c_time_string[100];
    TimeInfo *p_map;

    fd = open(file, O_RDWR | O_TRUNC | O_CREAT, 0777);
    if(fd < 0) {
        perror("open");
        exit(-1);
    }
    lseek(fd,sizeof(TimeInfo), SEEK_SET);
    write(fd, "", 1);

    p_map = (TimeInfo*)mmap(0, sizeof(TimeInfo), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//fprintf(stderr, "mmap address:%#x\n",(unsigned int)&p_map);	//0x00000
    close(fd);


    current_time = time(NULL);
    strcpy(c_time_string, ctime(&current_time));

    memcpy(p_map->c_time_string, &c_time_string , sizeof(c_time_string));

//    printf("initialize over\n ");

    munmap(p_map, sizeof(TimeInfo));
//    printf("umap ok \n");


    fd = open(file, O_RDWR);
    p_map = (TimeInfo*)mmap(0, sizeof(TimeInfo), PROT_READ, MAP_SHARED, fd, 0);
fprintf(stderr, "in mmap_write:");
fprintf(stderr, "%s", p_map->c_time_string);

    return 0;
}

char *mmap_read(char file[])
{
    int fd,i;
    time_t current_time;
    char c_time_string[100];
    TimeInfo *p_map;

    fd = open(file, O_RDWR);
    p_map = (TimeInfo*)mmap(0, sizeof(TimeInfo),  PROT_READ,  MAP_SHARED, fd, 0);
    
fprintf(stderr, "mmap_read:%s\n", p_map->c_time_string);

    return (p_map->c_time_string);
}


void sigusr1_handler(int signo) {
	output = 1;
	//count++;
//fprintf(stderr, "sigusr1 count is %d\n", count);
fprintf(stderr, "i get SIGUSR1\n");
}

void sigchld_handler(int signo) {
	sig = 1;
	count++;
fprintf(stderr, "sigchld count is %d\n", count);
fprintf(stderr, "i get SIGCHLD\n");
}


int main( int argc, char **argv ) {
    http_server server;             // http server
    http_request* requestP = NULL;  // pointer to http requests from client

    int maxfd;                      // size of open file descriptor table

    struct sockaddr_in cliaddr;     // used by accept()
    int clilen;

    int conn_fd;                    // fd for a new connection with client
    int err;                        // used by read_header_and_file()
    int ret, ret1, nwritten;

    struct stat sb;
    char timebuf[100];
    int buflen;
    char buf[10000];
    time_t now;

    char buffer[10000];
    int pid, status, already_map = 0, last_can_enter = 0;

    // Parse args. 
    if ( argc != 3 ) {
        (void)fprintf( stderr, "usage:  %s port# logfile\n", argv[0] );
        exit( 1 );
    }

    logfilenameP = argv[2];

    // Initialize http server
    init_http_server( &server, (unsigned short)atoi( argv[1] ) );

    maxfd = getdtablesize();
	//memset(sig, 0, sizeof(sig));

    requestP = (http_request *)malloc( sizeof( http_request ) * maxfd );
    if ( requestP == (http_request *)0 ) {
        fprintf( stderr, "out of memory allocating all http requests\n" );
        exit( 1 );
    }
    for ( int i = 0; i < maxfd; i++ )
        init_request( &requestP[i] );
    requestP[ server.listen_fd ].conn_fd = server.listen_fd;
    requestP[ server.listen_fd ].status = READING;

    fprintf( stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n", server.hostname, server.port, server.listen_fd, maxfd, logfilenameP );

    fd_set rset, rset_copy;
    FD_ZERO(&rset); FD_ZERO(&rset_copy);
    FD_SET(server.listen_fd, &rset);
    int change, file_fd[maxfd][2];

	struct sigaction act, act1, oact;
	act.sa_handler = sigusr1_handler;
	act1.sa_handler = sigchld_handler;
	sigemptyset(&act.sa_mask);
	sigemptyset(&act1.sa_mask);
	act.sa_flags = 0;
	act1.sa_flags = 0;
	if (sigaction(SIGUSR1, &act, &oact) < 0) {
		fprintf(stderr, "sigaction act error");
	}
	if (sigaction(SIGCHLD, &act1, &oact) < 0) {
		fprintf(stderr, "sigaction act1 error\n");
	}

    // Main loop. 
    while (1) {
        rset_copy = rset;

        // Wait for a connection.
	
        change = select(maxfd, &rset_copy, NULL, NULL, 0);
//fprintf(stderr, "change = %d\n", change);       
        if (change <= 0) continue;

        for (int i = 3; i < 100; i++) {
//fprintf(stderr, "int i = %d\n", i);
            if (FD_ISSET(i, &rset_copy)) {
//fprintf(stderr, "FD_ISSET %d\n", i);
                if (i == server.listen_fd) {
//fprintf(stderr, "i == server.listen_fd == %d\n", i);
                    clilen = sizeof(cliaddr);
                    conn_fd = accept( server.listen_fd, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen );
                    if ( conn_fd < 0 ) {
					    if ( errno == EINTR || errno == EAGAIN ) continue; // try again 
                        if ( errno == ENFILE ) {
                            (void)fprintf( stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd );
                            continue;
                        }
                        ERR_EXIT( "accept" )
                    }
                    requestP[conn_fd].conn_fd = conn_fd;    //conn_fd = 第幾個requestP = 第幾個requestP的conn_fd = 寫資料到這裡可以拿給第幾個requestP
//fprintf(stderr, "i have already make the requestP[%d].conn_fd = %d\n", conn_fd, conn_fd);
                    requestP[conn_fd].status = READING;
                    
                    strcpy( requestP[conn_fd].host, inet_ntoa( cliaddr.sin_addr ) );
                    set_ndelay( conn_fd );

                    fprintf( stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host );

                    while (1) {
                        ret = read_header_and_file( &requestP[conn_fd], &err );
                        if ( ret > 0 ) continue;
                        else if ( ret < 0 ) {
                            // error for reading http header or requested file
                            fprintf( stderr, "error on fd %d, code %d\n", requestP[conn_fd].conn_fd, err );
                            requestP[conn_fd].status = ERROR;
                            close( requestP[conn_fd].conn_fd );
                            free_request( &requestP[conn_fd] );
                            break;
                        }
                        else if ( ret == 0 ) {
                            // ready for writing
                            fprintf( stderr, "writing (buf %p, idx %d) %d bytes to request fd %d\n", requestP[conn_fd].buf, (int)requestP[conn_fd].buf_idx, (int)requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd );

                            char filename[MAXBUFSIZE], tmp[MAXBUFSIZE], file_reader[MAXBUFSIZE];
                            sscanf(requestP[conn_fd].query, "filename=%s", filename);
                            sscanf(requestP[conn_fd].file, "%s", tmp);
                            sprintf(file_reader, "./%s", tmp);

//fprintf(stderr, "filename:%s\n", filename);
//fprintf(stderr, "file_reader:%s\n", file_reader);
							if (strcmp(tmp, "info") == 0) {
								int pid_info;
								sigset_t new, old, wait;
								sigemptyset(&new);
								sigaddset(&new, SIGUSR1);
								sigaddset(&new, SIGCHLD);
								sigemptyset(&old);
								sigprocmask(SIG_BLOCK, &new, &old);
								sigemptyset(&wait);
								sigaddset(&wait, SIGCHLD);
								if ((pid_info = fork()) == 0) {
									if (execlp("./info", "./info", (char *)NULL) < 0)
										printf("info.c error\n");
								}
								while (!output) {
//fprintf(stderr, "i'm waiting signal\n");
									if (sigsuspend(&wait) != -1) {
										fprintf(stderr, "sigsuspend error");
									}
								}
								if (output == 1) {
									int len = 0;
									char buffer1[50], buffer2[50], buffer3[50], buffer4[50], temp[50];
									if (already_map == 0) {
										sprintf(buffer1, "0 process died previously.\n");
										sprintf(buffer2, "PIDs of Running Processes: ");
										sprintf(temp, "\n");
										sprintf(buffer3, "Last Exit CGI: \n");
										sprintf(buffer4, "Last Exit Filename: \n");
										len = strlen(buffer1) + strlen(buffer2) + strlen(buffer3) + strlen(buffer4) + strlen(temp);
fprintf(stderr, "buffer1:%d\n", strlen(buffer1));
fprintf(stderr, "buffer2:%d\n", strlen(buffer2));
fprintf(stderr, "buffer3:%d\n", strlen(buffer3));
fprintf(stderr, "buffer4:%d\n", strlen(buffer4));
fprintf(stderr, "temp:%d\n", strlen(temp));
										for (int j = 0; j < maxfd; j++) {
												if (requestP[j].pid_on == 1) {
														sprintf(temp, "%d ", requestP[j].pid);
														len += strlen(temp);
fprintf(stderr, "temp:%d\n", strlen(temp));
												}
										}
	
										requestP[conn_fd].buf_len = 0;
										requestP[conn_fd].buf_size = 0;
									
										buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										now = time( (time_t *)0 );
										(void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
										buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t)len );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
										add_to_buf( &requestP[conn_fd], buf, buflen );
	
										buflen = snprintf( buf, sizeof(buf), "%s", buffer1);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer2);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										for (int j = 0; j < maxfd; j++) {
						 						if (requestP[j].pid_on == 1) {
														sprintf(temp, "%d ", requestP[j].pid);
														buflen = snprintf( buf, sizeof(buf), "%s", temp);
														add_to_buf( &requestP[conn_fd], buf, buflen );
												}
										}
										sprintf(temp, "\n");
										buflen = snprintf( buf, sizeof(buf), "%s", temp);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer3);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer4);
										add_to_buf( &requestP[conn_fd], buf, buflen );
		
										nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
										nwritten = write( 1, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
										
										//FD_CLR(i, &rset);
										fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[conn_fd].conn_fd );
										
										close( requestP[conn_fd].conn_fd );
										free_request( &requestP[conn_fd] );

										mmap_write(logfilenameP);
										already_map = 1;
										count++;
fprintf(stderr, "i finsih info and now count is %d\n", count);
	
										output = 0;
										break;
									}
									else {
										sprintf(buffer1, "%d processes died previously.\n", count);
										sprintf(buffer2, "PIDs of Running Processes: ");
										sprintf(temp, "\n");
										if (last_can_enter == 0) {//info->info bad->info
											sprintf(buffer3, "Last Exit CGI: \n");
											sprintf(buffer4, "Last Exit filename: \n\0");
										}
										else if (last_can_enter == 1) {//ok->info
											sprintf(buffer3, "Last Exit CGI:%s\n", mmap_read(logfilenameP));
											sprintf(buffer4, "Last Exit filename: %s\n", filename);
										}
										else if (last_can_enter == 2) {//not->info
											sprintf(buffer3, "Last Exit CGI:%s\n", mmap_read(logfilenameP));
											sprintf(buffer4, "The file tried last time is not found!\n\0");
										}
										len = strlen(buffer1) + strlen(buffer2) + strlen(buffer3) + strlen(buffer4) + strlen(temp);
fprintf(stderr, "buffer1:%d, buffer2:%d, buffer3:%d, buffer4:%d, tmp:%d\n", strlen(buffer1), strlen(buffer2), strlen(buffer3), strlen(temp));
										for (int j = 0; j < maxfd; j++) {
												if (requestP[j].pid_on == 1) {
														sprintf(temp, "%d ", requestP[j].pid);
														len += strlen(temp);
fprintf(stderr, "temp:%d\n", strlen(temp));
												}
										}

										requestP[conn_fd].buf_len = 0;
										requestP[conn_fd].buf_size = 0;
					
										buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										now = time( (time_t *)0 );
										(void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
										buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t)len );
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
										add_to_buf( &requestP[conn_fd], buf, buflen );

										buflen = snprintf( buf, sizeof(buf), "%s", buffer1);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer2);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										for (int j = 0; j < maxfd; j++) {
												if (requestP[j].pid_on == 1) {
														sprintf(temp, "%d ", requestP[j].pid);
														buflen = snprintf( buf, sizeof(buf), "%s", temp);
														add_to_buf( &requestP[conn_fd], buf, buflen );
												}
										}
										sprintf(temp, "\n");
										buflen = snprintf( buf, sizeof(buf), "%s", temp);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer3);
										add_to_buf( &requestP[conn_fd], buf, buflen );
										buflen = snprintf( buf, sizeof(buf), "%s", buffer4);
										add_to_buf( &requestP[conn_fd], buf, buflen );

										nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
										nwritten = write( 1, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
	
										fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[conn_fd].conn_fd );

										close( requestP[conn_fd].conn_fd );
										free_request( &requestP[conn_fd] );
	
										mmap_write(logfilenameP);
										already_map = 1;
										last_can_enter = 0;
										count++;
fprintf(stderr, "i finish info and now count is %d\n", count);
	
										output = 0;
	
										break;
									}
								}
							}

                            if (!valid(filename)) {
                                requestP[conn_fd].buf_len = 0;
                                requestP[conn_fd].buf_size = 0;

                                //memset(requestP[conn_fd].buf, 0, *requestP[conn_fd].buf);
                                

                                buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 400 BAD REQUEST\015\012Server: SP TOY\015\012" );
                                add_to_buf( &requestP[conn_fd], buf, buflen );
                                now = time( (time_t *)0 );
                                (void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
                                buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
                                add_to_buf( &requestP[conn_fd], buf, buflen );
                                buflen = snprintf( buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t)15 );
                                add_to_buf( &requestP[conn_fd], buf, buflen );
                                buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
                                add_to_buf( &requestP[conn_fd], buf, buflen );

                                buflen = snprintf( buf, sizeof(buf), "400 BAD REQUEST\n" );
                                add_to_buf( &requestP[conn_fd], buf, buflen );

                                nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
                            	nwritten = write( 1, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
								close( requestP[conn_fd].conn_fd );
								free_request( &requestP[conn_fd] );

								mmap_write(logfilenameP);
								already_map = 1;
								last_can_enter = 0;

								fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[conn_fd].conn_fd );
                            }
                            else {
								sigset_t new, old, wait;
								sigemptyset(&new);
								sigaddset(&new, SIGCHLD);
								sigaddset(&new, SIGUSR1);
								sigemptyset(&old);
								sigprocmask(SIG_BLOCK, &new, &old);
								sigemptyset(&wait);
								sigaddset(&wait, SIGUSR1);

                                pipe(file_fd[conn_fd]);
                                
                                if ((requestP[conn_fd].pid = fork()) == 0) {
                                    dup2(file_fd[conn_fd][0], 0);
                                    dup2(file_fd[conn_fd][1], 1);
                                    close(file_fd[conn_fd][0]);
                                    close(file_fd[conn_fd][1]);
                                    if (execlp(file_reader, file_reader, filename, (char *)NULL) < 0)
                                        printf("execlp_file_reader.c error\n");
                                }

								while (!sig) {
//fprintf(stderr, "i'm waiting signal\n");
									if (sigsuspend(&wait) != -1) {
										fprintf(stderr, "sigsuspend error");
									}
								}
								if (sig == 1) {
									requestP[conn_fd].pid_on = 1;
fprintf(stderr, "requestP[%d].pid_on = 1\n", conn_fd);

									FD_SET(file_fd[conn_fd][0], &rset);
                                	requestP[file_fd[conn_fd][0]].conn_fd = conn_fd;
//fprintf(stderr, "double check, FD_Set %d into rset\n", file_fd[conn_fd][0]);
//fprintf(stderr, "double check conn_fd %d\n", conn_fd);
									close(file_fd[conn_fd][1]);
								}
                            }
                            // write once only and ignore error
                            break;
                        }
                    }
                }
				else {
                //else if (i != requestP[i].conn_fd && requestP[i].conn_fd != -1) {
//fprintf(stderr, "how many times i go into\n");
					conn_fd = requestP[i].conn_fd;
//fprintf(stderr, "why are you here// i: %d\n", i);
//fprintf(stderr, "why are you still here// conn_Fd:  %d\n", conn_fd);
                    if (waitpid(requestP[conn_fd].pid, &status, 0) == -1) {
//fprintf(stderr, "here first\n");
                    	perror("waitpid() failed");
                        exit(EXIT_FAILURE);
                    }
                    int es;
                    if ( WIFEXITED(status) ) {
                    	es = WEXITSTATUS(status);
//fprintf(stderr, "here after\n");
                        fprintf(stderr, "Exit status was %d\n", es);
                    }
                    if (es != 0) {
                    	requestP[conn_fd].buf_len = 0;
                        requestP[conn_fd].buf_size = 0;

                        //memset(requestP[conn_fd].buf, 0, *requestP[conn_fd].buf);
                                    
						buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 404 NOT FOUND\015\012Server: SP TOY\015\012" );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        now = time( (time_t *)0 );
                        (void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
                        buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        buflen = snprintf( buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t)13 );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
                        add_to_buf( &requestP[conn_fd], buf, buflen );

                        buflen = snprintf( buf, sizeof(buf), "404 NOT FOUND\n" );
                        add_to_buf( &requestP[conn_fd], buf, buflen );

                        nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
                        nwritten = write( 1, requestP[conn_fd].buf, requestP[conn_fd].buf_len );

						requestP[conn_fd].pid_on = 0;
fprintf(stderr, "requestP[%d].pid_on = 0 and the file is not found\n", conn_fd);

                        FD_CLR(i, &rset);
						close( requestP[conn_fd].conn_fd );
						free_request( &requestP[conn_fd] );

						mmap_write(logfilenameP);
						already_map = 1;
						last_can_enter = 2;

						sig = 0;

						fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[conn_fd].conn_fd );
					}
                    else {
//fprintf(stderr, "im here if es == 0\n");
						requestP[conn_fd].buf_len = 0;
                        requestP[conn_fd].buf_size = 0;
                        
//fprintf(stderr, "is here OK?\n");
						//memset(requestP[conn_fd].buf, 0, sizeof(*requestP[conn_fd].buf));
//fprintf(stderr, "is here OK?\n");
						ret1 = read(i, buffer, sizeof(buffer));
						close(i);
       
//fprintf(stderr, "is here OK?\n");
                        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        now = time( (time_t *)0 );
                        (void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
                        buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        buflen = snprintf( buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t)strlen(buffer) );
                        add_to_buf( &requestP[conn_fd], buf, buflen );
                        buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
                        add_to_buf( &requestP[conn_fd], buf, buflen );

//fprintf(stderr, "i:%d\n", i);
//fprintf(stderr, "conn_Fd:%d\n", conn_fd);
//fprintf(stderr, "buffer:%s\n", buffer);
                        buflen = snprintf( buf, sizeof(buf), "%s", buffer);
                        add_to_buf( &requestP[conn_fd], buf, buflen );

                        nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len );
                    	nwritten = write( 1, requestP[conn_fd].buf, requestP[conn_fd].buf_len );

						requestP[conn_fd].pid_on = 0;
fprintf(stderr, "requestP[%d].pid_on = 0\n", conn_fd);

//fprintf(stderr, "FD_CLR %d\n", i);
						FD_CLR(i, &rset);
						fprintf( stderr, "complete writing %d bytes on fd %d\n", nwritten, requestP[conn_fd].conn_fd );
						close( requestP[conn_fd].conn_fd );
						free_request( &requestP[conn_fd] );

						mmap_write(logfilenameP);
						already_map = 1;
						last_can_enter = 1;

						sig = 0;
					}
                }
            }
        }
        
    }
    free( requestP );
    return 0;
}

int valid(char filename[128]) {
    int len = strlen(filename);
    for (int i = 0; i < len; i++) {
        if ((filename[i] >= 'a' && filename[i] <= 'z') || (filename[i] >= 'A' && filename[i] <= 'Z') || (filename[i] >= '0' && filename[i] <= '9') || filename[i] == '_') continue;
        else return 0;
    }
    return 1;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request( http_request* reqP ) {
    reqP->conn_fd = -1;
    reqP->status = 0;           // not used
    reqP->file[0] = (char)0;
    reqP->query[0] = (char)0;
    reqP->host[0] = (char)0;
    reqP->buf = NULL;
    reqP->buf_size = 0;
    reqP->buf_len = 0;
    reqP->buf_idx = 0;
}

static void free_request( http_request* reqP ) {
    if ( reqP->buf != NULL ) {
        free( reqP->buf );
        reqP->buf = NULL;
    }
    init_request( reqP );
}


#define ERR_RET( error ) { *errP = error; return -1; }
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected
//
static int read_header_and_file( http_request* reqP, int *errP ) {
    // Request variables
    char *file = (char *)0;
    char *path = (char *)0;
    char *query = (char *)0;
    char *protocol = (char *)0;
    char *method_str = (char *)0;
    int r, fd;
    struct stat sb;
    char timebuf[100];
    int buflen;
    char buf[10000];
    time_t now;
    void *ptr;

    // Read in request from client
    while (1) {
        r = read( reqP->conn_fd, buf, sizeof(buf) );
        if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) ) return 1;
        if ( r <= 0 ) ERR_RET( 1 )
        add_to_buf( reqP, buf, r );
        if ( strstr( reqP->buf, "\015\012\015\012" ) != (char*)0 || strstr( reqP->buf, "\012\012" ) != (char*)0 ) break;
    }
    // fprintf( stderr, "header: %s\n", reqP->buf );

    // Parse the first line of the request.
    method_str = get_request_line( reqP );
    if ( method_str == (char *)0 ) ERR_RET( 2 )
    path = strpbrk( method_str, " \t\012\015" );
    if ( path == (char *)0 ) ERR_RET( 2 )
    *path++ = '\0';
    path += strspn( path, " \t\012\015" );
    protocol = strpbrk( path, " \t\012\015" );
    if ( protocol == (char *)0 ) ERR_RET( 2 )
    *protocol++ = '\0';
    protocol += strspn( protocol, " \t\012\015" );
    query = strchr( path, '?' );
    if ( query == (char *)0 ) query = "";
    else *query++ = '\0';

    if ( strcasecmp( method_str, "GET" ) != 0 ) ERR_RET( 3 )
    else {
        strdecode( path, path );
        if ( path[0] != '/' ) ERR_RET( 4 )
    else file = &(path[1]);
    }

    if ( strlen( file ) >= MAXBUFSIZE - 1 ) ERR_RET( 4 )
    if ( strlen( query ) >= MAXBUFSIZE - 1 ) ERR_RET( 5 )
      
    strcpy( reqP->file, file );
    strcpy( reqP->query, query );

    /*
    if ( query[0] == (char)0 ) {
        // for file request, read it in buf
        r = stat( reqP->file, &sb );
        if ( r < 0 ) ERR_RET( 6 )

        fd = open( reqP->file, O_RDONLY );
        if ( fd < 0 ) ERR_RET( 7 )

        reqP->buf_len = 0;

        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
        add_to_buf( reqP, buf, buflen );
        now = time( (time_t *)0 );
        (void)strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
        buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
        add_to_buf( reqP, buf, buflen );
        buflen = snprintf( buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t)sb.st_size );
        add_to_buf( reqP, buf, buflen );
        buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
        add_to_buf( reqP, buf, buflen );

        ptr = mmap( 0, (size_t) sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
        if ( ptr == (void *)-1 ) ERR_RET( 8 )
        add_to_buf( reqP, ptr, sb.st_size );
        (void)munmap( ptr, sb.st_size );
        close( fd );
        // printf( "%s\n", reqP->buf );
        // fflush( stdout );
        reqP->buf_idx = 0; // writing from offset 0
        return 0;
    }
    */

    return 0;
}


static void add_to_buf( http_request *reqP, char* str, size_t len ) { 
    char **bufP = &(reqP->buf);
    size_t *bufsizeP = &(reqP->buf_size);
    size_t *buflenP = &(reqP->buf_len);

    if ( *bufsizeP == 0 ) {
        *bufsizeP = len + 500;
        *buflenP = 0;
        *bufP = (char *)e_malloc( *bufsizeP );
    }
    else if ( *buflenP + len >= *bufsizeP ) {
        *bufsizeP = *buflenP + len + 500;
        *bufP = (char *)e_realloc( (void *)*bufP, *bufsizeP );
    }
    (void)memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}

static char* get_request_line( http_request *reqP ) { 
    int begin;
    char c;

    char *bufP = reqP->buf;
    int buf_len = reqP->buf_len;

    for ( begin = reqP->buf_idx; reqP->buf_idx < buf_len; ++reqP->buf_idx ) {
        c = bufP[ reqP->buf_idx ];
        if ( c == '\012' || c == '\015' ) {
            bufP[reqP->buf_idx] = '\0';
            ++reqP->buf_idx;
            if ( c == '\015' && reqP->buf_idx < buf_len && bufP[reqP->buf_idx] == '\012' ) {
                bufP[reqP->buf_idx] = '\0';
                ++reqP->buf_idx;
            }
            return &(bufP[begin]);
        }
    }
    fprintf( stderr, "http request format error\n" );
    exit( 1 );
}



static void init_http_server( http_server *svrP, unsigned short port ) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname( svrP->hostname, sizeof( svrP->hostname) );
    svrP->port = port;
   
    svrP->listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( svrP->listen_fd < 0 ) ERR_EXIT( "socket" )

    bzero( &servaddr, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( port );
    tmp = 1;
    if ( setsockopt( svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&tmp, sizeof(tmp) ) < 0 ) ERR_EXIT ( "setsockopt " )
    if ( bind( svrP->listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0 ) ERR_EXIT( "bind" )

    if ( listen( svrP->listen_fd, 1024 ) < 0 ) ERR_EXIT( "listen" )
}

// Set NDELAY mode on a socket.
static void set_ndelay( int fd ) {
    int flags, newflags;

    flags = fcntl( fd, F_GETFL, 0 );
    if ( flags != -1 ) {
        newflags = flags | (int)O_NDELAY; // nonblocking mode
        if ( newflags != flags ) (void)fcntl( fd, F_SETFL, newflags );
    }
}   

static void strdecode( char *to, char *from ) {
    for ( ; *from != '\0'; ++to, ++from ) {
        if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
            *to = hexit( from[1] ) * 16 + hexit( from[2] );
            from += 2;
        }
        else {
            *to = *from;
        }
    }
    *to = '\0';
}


static int hexit( char c ) {
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    return 0;   // shouldn't happen
}


static void *e_malloc( size_t size ) {
    void *ptr;

    ptr = malloc( size );
    if ( ptr == (void *)0 ) {
        (void)fprintf( stderr, "out of memory\n" );
        exit( 1 );
    }
    return ptr;
}


static void *e_realloc( void *optr, size_t size ) {
    void *ptr;

    ptr = realloc( optr, size );
    if ( ptr == (void *)0 ) {
        (void)fprintf( stderr, "out of memory\n" );
        exit( 1 );
    }
    return ptr;
}
