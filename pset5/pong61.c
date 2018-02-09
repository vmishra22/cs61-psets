#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include "serverinfo.h"
#include "pthread.h"

static const char *pong_host = PONG_HOST;
static const char *pong_port = PONG_PORT;
static const char *pong_user = PONG_USER;


// Timer and interrupt functions (defined and explained below)

double timestamp(void);
void sleep_for(double delay);
void interrupt_after(double delay);
void interrupt_cancel(void);

int numConnections = 0;
int numThreads = 0;

int x = 0, y = 0, dx = 1, dy = 1;
int width, height;

//static ssize_t numBufRead = 0;
// HTTP connection management functions

// http_connection
//    This object represents an open HTTP connection to a server.
typedef struct http_connection {
    int fd;                 // Socket file descriptor

    int state;              // Response parsing status (see below)
    int status_code;        // Response status code (e.g., 200, 402)
    size_t content_length;  // Content-Length value
    int has_content_length; // 1 iff Content-Length was provided

    char buf[BUFSIZ];       // Response buffer
    size_t len;             // Length of response buffer
} http_connection;

 pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    
// `http_connection::state` constants:
#define HTTP_REQUEST 0      // Request not sent yet
#define HTTP_INITIAL 1      // Before first line of response
#define HTTP_HEADERS 2      // After first line of response, in headers
#define HTTP_BODY    3      // In body
#define HTTP_DONE    (-1)   // Body complete, available for a new request
#define HTTP_CLOSED  (-2)   // Body complete, connection closed
#define HTTP_BROKEN  (-3)   // Parse error

// helper functions
char *http_truncate_response(http_connection *conn);
static int http_consume_headers(http_connection *conn, int eof);

static void usage(void);

// http_connect(ai)
//    Open a new connection to the server described by `ai`. Returns a new
//    `http_connection` object for that server connection. Exits with an
//    error message if the connection fails.
http_connection *http_connect(const struct addrinfo *ai) {
    // connect to the server
    
    if( numConnections > 30)
        return NULL;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    int r = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (r < 0) {
        perror("connect");
        exit(1);
    }

    // construct an http_connection object for this connection
    http_connection *conn = (http_connection *) malloc(sizeof(http_connection));
    conn->fd = fd;
    conn->state = HTTP_REQUEST;
    numConnections++;
    printf("Num Connections: %d\n", numConnections);
    return conn;
}


// http_close(conn)
//    Close the HTTP connection `conn` and free its resources.
void http_close(http_connection *conn) {
    close(conn->fd);
    free(conn);
    numConnections--;
}


// http_send_request(conn, uri)
//    Send an HTTP POST request for `uri` to connection `conn`.
//    Exit on error.
void http_send_request(http_connection *conn, const char *uri) {
    assert(conn->state == HTTP_REQUEST || conn->state == HTTP_DONE);

    // prepare and write the request
    char reqbuf[BUFSIZ];
    size_t reqsz = sprintf(reqbuf,
                           "POST /%s/%s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n",
                           pong_user, uri, pong_host);
    size_t pos = 0;
    while (pos < reqsz) {
        ssize_t nw = write(conn->fd, &reqbuf[pos], reqsz - pos);
        if (nw == 0)
            break;
        else if (nw == -1 && errno != EINTR && errno != EAGAIN) {
            perror("write");
            exit(1);
        } else if (nw != -1)
            pos += nw;
    }

    if (pos != reqsz) {
        fprintf(stderr, "connection closed prematurely\n");
        exit(1);
    }

    // clear response information
    conn->state = HTTP_INITIAL;
    conn->status_code = -1;
    conn->content_length = 0;
    conn->has_content_length = 0;
    conn->len = 0;
}

void thread_work_function(http_connection *conn)
{ 
    
    printf("thread_work_function\n");

    
   
    ssize_t nr = 0;
    pthread_mutex_lock(&lock);
 if(conn->state != HTTP_DONE)
        {
 
    printf("Thread conn->len 1 is %d.\n", conn->len);
     printf("Thread conn->status_code1 = %d \n", conn->status_code);
     printf("Thread conn->state 1:  %d\n", conn->state);
    size_t eof = 0;
    while (http_consume_headers(conn, eof)) 
    { 
        //interrupt_after(1.0);
        
        printf("Thread conn->status_code2 = %d \n", conn->status_code);
        printf("Thread conn->state 2:  %d\n", conn->state);
        nr = read(conn->fd, &conn->buf[conn->len], BUFSIZ);
        
        printf("Thread nr is %d.\n", nr);
        
        if (nr == 0)
            eof = 1;
        else if (nr != -1)
            conn->len += nr;
        
    }
               
    if(conn->status_code == 200 && nr >= 0)
    {
        conn->len += nr;
        conn->buf[conn->len] = 0;
        
        printf("Thread conn->len 2 is %d.\n", conn->len);
        printf("Thread Incremented X\n");
        x += dx;
        y += dy;
        if (x < 0 || x >= width) {
            dx = -dx;
            x += 2 * dx;
        }
        if (y < 0 || y >= height) {
            dy = -dy;
            y += 2 * dy;
        }
        
    }
    pthread_mutex_unlock(&lock);
    }
     
    printf("Out of thread function\n");
}

void *thread_routine(void *arg) 
{
    http_connection *threadConn = (http_connection*) arg;
    printf("thread_routine\n");
    
    pthread_detach(pthread_self());
    

    thread_work_function(threadConn);
    
     printf("Closing In thread\n");
     if(threadConn->status_code == 200 && threadConn->state == HTTP_DONE) 
     {
       // pthread_exit(NULL);
       http_close(threadConn);
     }


    return NULL;
}

// http_receive_response(conn)
//    Receive a response from the server. On return, `conn->status_code`
//    holds the server's status code, and `conn->buf` holds the response
//    body, which is `conn->len` bytes long and has been null-terminated.
//    If the connection terminated prematurely, `conn->status_code`
//    is -1.
void http_receive_response(http_connection *conn, int needThreads, int *closeConnection) {
    assert(conn->state != HTTP_REQUEST);
    if (conn->state < 0)
        return;

    // parse connection (http_consume_headers tells us when to stop)
    size_t eof = 0;
    while (http_consume_headers(conn, eof)) 
    { 
            printf("In response fn conn->status_code1 = %d \n", conn->status_code);
            printf("conn->state response 1:  %d\n", conn->state);
           
            if( needThreads && conn->state == HTTP_BODY && conn->status_code != -1 && errno == EINTR)
            {
                printf("********In Thread:*******\n");
               // interrupt_cancel();
                 printf("conn->len %d\n", conn->len);
                pthread_t t;
                int ret = pthread_create(&t, NULL, thread_routine, conn);
                printf("Thread Created:  %d\n", ret);
                if(!ret)
                    *closeConnection = 0;
            }
            else
            {
            
                interrupt_after(1.0);
                ssize_t nr = read(conn->fd, &conn->buf[conn->len], BUFSIZ);
                
                printf(" String read %s\n", conn->buf);
                printf("Num Read %d\n", nr);
                printf("conn->state response 2:  %d\n", conn->state);

/*                if(errno  == EINTR)*/
/*                {*/
/*                    printf("Error Reading"); */
/*                }*/
/*                else if( errno  == EAGAIN)*/
/*                {*/
/*                    printf("EAGAIN");*/
/*                }*/
/*                */
/*                printf("In response fn conn->status_code2 = %d \n", conn->status_code);*/
                if (nr == 0)
                    eof = 1;
                else if (nr == -1 && errno != EINTR && errno != EAGAIN) 
                {
                    perror("read");
                    exit(1);
                } 
                else if (nr != -1)
                    conn->len += nr;
                    
                printf("conn->len %d\n", conn->len);
            }
    }   

    // null-terminate body
    conn->buf[conn->len] = 0;
    
    printf("conn->state response 3:  %d\n", conn->state);
    
    if(conn->status_code == 200)
        *closeConnection = 1;

    // Status codes >= 500 mean we are overloading the server and should exit.
    if (conn->status_code >= 500) 
    {
        fprintf(stderr, "exiting because of server status %d (%s)\n", conn->status_code, http_truncate_response(conn));
        exit(1);
    }
}


// http_truncate_response(conn)
//    Truncate the `conn` response text to a manageable length and return
//    that truncated text. Useful for error messages.
char *http_truncate_response(http_connection *conn) {
    char *eol = strchr(conn->buf, '\n');
    if (eol)
        *eol = 0;
    if (strnlen(conn->buf, 100) >= 100)
        conn->buf[100] = 0;
    return conn->buf;
}


// main(argc, argv)
//    The main loop.
int main(int argc, char **argv) {
    // parse arguments
    int ch;
    while ((ch = getopt(argc, argv, "h:p:u:")) != -1) {
        if (ch == 'h')
            pong_host = optarg;
        else if (ch == 'p')
            pong_port = optarg;
        else if (ch == 'u')
            pong_user = optarg;
        else
            usage();
    }
    if (optind == argc - 1)
        pong_user = argv[optind];
    else if (optind != argc)
        usage();

    // look up network address of pong server
    struct addrinfo ai_hints, *ai;
    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_INET;
    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_flags = AI_NUMERICSERV;
    int r = getaddrinfo(pong_host, pong_port, &ai_hints, &ai);
    if (r != 0) {
        fprintf(stderr, "problem contacting %s: %s\n",
                pong_host, gai_strerror(r));
        exit(1);
    }
    
    int needCloseConnectionInMain = -1;

    int needThreads = 0;
    // reset pong board and get its dimensions
    
    {
        http_connection *conn = http_connect(ai);
        http_send_request(conn, "reset");
        http_receive_response(conn, needThreads, &needCloseConnectionInMain);
        if (conn->status_code != 200
            || sscanf(conn->buf, "%d %d\n", &width, &height) != 2
            || width <= 0 || height <= 0) {
            fprintf(stderr, "bad response to \"reset\" RPC: %d %s\n",
                    conn->status_code, http_truncate_response(conn));
            exit(1);
        }
        http_close(conn);
        needThreads++;
    }

    // print display URL
    printf("Display: http://%s:%s/%s/\n", pong_host, pong_port, pong_user);

    // play game
    
    char url[BUFSIZ];
    while (1) {
        http_connection *conn = http_connect(ai);
        
        //printf("conn->state main1:  %d\n", conn->state);

        sprintf(url, "move?x=%d&y=%d&style=on", x, y);
        http_send_request(conn, url);
        
        //printf("conn->state main2:  %d\n", conn->state);
        
        printf("In Main 1 fn conn->status_code = %d \n", conn->status_code);

        interrupt_after(1.0);
        http_receive_response(conn, needThreads, &needCloseConnectionInMain);
        
        //printf("conn->state main3:  %d\n", conn->state);
        printf("In Main 2 fn conn->status_code = %d \n", conn->status_code);
      
       double Ksec = 0.001;
       int countAttempt = 1;
       
       while(conn->status_code == -1)
       {
            printf("In Main 3 fn conn->status_code = %d \n", conn->status_code);
            printf("ConnectionFailed");
            http_close(conn);
            sleep_for(Ksec*countAttempt);
           
            conn = http_connect(ai);
            http_send_request(conn, url);
            
            printf("conn->state while:  %d\n", conn->state);
            
            printf("In Main 4 fn conn->status_code = %d \n", conn->status_code);
            http_receive_response(conn, needThreads, &needCloseConnectionInMain);
            
            printf("In Main 5 fn conn->status_code = %d \n", conn->status_code);
            countAttempt = countAttempt*2;
       }
        
        if (conn->status_code != 200)
            fprintf(stderr, "warning: %d,%d: server returned status %d "
                    "(expected 200)\n", x, y, conn->status_code);

        double result = strtod(conn->buf, NULL);
        if (result < 0) {
            fprintf(stderr, "server returned error: %s\n",
                    http_truncate_response(conn));
            exit(1);
        }

        printf("Incremented X\n");
        x += dx;
        y += dy;
        if (x < 0 || x >= width) {
            dx = -dx;
            x += 2 * dx;
        }
        if (y < 0 || y >= height) {
            dy = -dy;
            y += 2 * dy;
        }

        printf("conn->state main4:  %d\n", conn->state);
        printf("In Main 6 fn conn->status_code = %d \n", conn->status_code);
        // wait 0.1sec before moving to next frame
         interrupt_cancel();
        sleep_for(0.1);
        printf("needCloseConnectionInMain: %d\n", needCloseConnectionInMain);
       
        if(needCloseConnectionInMain)
        {
        http_close(conn);
        }
            //http_close(conn);
    }
}


// TIMING AND INTERRUPT FUNCTIONS
static void handle_sigalrm(int signo);

// timestamp()
//    Return the current time as a real number of seconds.
double timestamp(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + (double) now.tv_usec / 1000000;
}


// sleep_for(delay)
//    Sleep for `delay` seconds, or until an interrupt, whichever comes
//    first.
void sleep_for(double delay) {
    usleep((long) (delay * 1000000));
}


// interrupt_after(delay)
//    Cause an interrupt to occur after `delay` seconds. This interrupt will
//    make any blocked `read` system call terminate early, without returning
//    any data.
void interrupt_after(double delay) {
    static int signal_set = 0;
    if (!signal_set) {
        struct sigaction sa;
        sa.sa_handler = handle_sigalrm;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        int r = sigaction(SIGALRM, &sa, NULL);
        if (r < 0) {
            perror("sigaction");
            exit(1);
        }
        signal_set = 1;
    }

    struct itimerval timer;
    timerclear(&timer.it_interval);
    timer.it_value.tv_sec = (long) delay;
    timer.it_value.tv_usec = (long) ((delay - timer.it_value.tv_sec) * 1000000);
    int r = setitimer(ITIMER_REAL, &timer, NULL);
    if (r < 0) {
        perror("setitimer");
        exit(1);
    }
}


// interrupt_cancel()
//    Cancel any outstanding interrupt.
void interrupt_cancel(void) {
    struct itimerval timer;
    timerclear(&timer.it_interval);
    timerclear(&timer.it_value);
    int r = setitimer(ITIMER_REAL, &timer, NULL);
    if (r < 0) {
        perror("setitimer");
        exit(1);
    }
}


// This is a helper function for `interrupt_after`.
static void handle_sigalrm(int signo) {
    (void) signo;
}


// HELPER FUNCTIONS FOR CODE ABOVE

// http_consume_headers(conn, eof)
//    Parse the response represented by `conn->buf`. Returns 1
//    if more data should be read into `conn->buf`, 0 if the response is
//    complete.
static int http_consume_headers(http_connection *conn, int eof) {
    size_t i = 0;
    while ((conn->state == HTTP_INITIAL || conn->state == HTTP_HEADERS)
           && i + 2 <= conn->len) {
        if (conn->buf[i] == '\r' && conn->buf[i+1] == '\n') {
            conn->buf[i] = 0;
            if (conn->state == HTTP_INITIAL) {
                int minor;
                if (sscanf(conn->buf, "HTTP/1.%d %d",
                           &minor, &conn->status_code) == 2)
                    conn->state = HTTP_HEADERS;
                else
                    conn->state = HTTP_BROKEN;
            } else if (i == 0)
                conn->state = HTTP_BODY;
            else if (strncmp(conn->buf, "Content-Length: ", 16) == 0) {
                conn->content_length = strtoul(conn->buf + 16, NULL, 0);
                conn->has_content_length = 1;
            }
            memmove(conn->buf, conn->buf + i + 2, conn->len - (i + 2));
            conn->len -= i + 2;
            i = 0;
        } else
            ++i;
    }

    if (conn->state == HTTP_BODY
        && (conn->has_content_length || eof)
        && conn->len >= conn->content_length)
        conn->state = HTTP_DONE;
    if (eof)
        conn->state = (conn->state == HTTP_DONE ? HTTP_CLOSED : HTTP_BROKEN);
    return conn->state >= 0;
}


// usage()
//    Explain how pong61 should be run.
static void usage(void) {
    fprintf(stderr, "Usage: ./pong61 [-h HOST] [-p PORT] [USER]\n");
    exit(1);
}
