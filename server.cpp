/*
    Server Application for CMS
    Copyright (C) 2020  Ameya Mohape 
    mohapeameya@gmail.com
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
command codes sent by client
CONNECT    0
CREATE     1
UPDATE     2
READ       3
DELETE     4
DISCONNECT 5

status codes sent by server
OK         0
NOT_OK     1
*/

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <arpa/inet.h>

using namespace std;

#define BUFFER_SIZE 1024 // fd buffer size
#define COUNT 400000 // level cpu intensive work

//#define WORKERS 4

int fd_buffer[BUFFER_SIZE];
char *serv_ip;

map<int,string> key_value;

pthread_mutex_t fd_mutex;
pthread_mutex_t kv_mutex;

pthread_cond_t empty, full;
int front, rear;

void *accept_conn(void *array) {
    int *data = (int *)array;
    // int thread_id;
    int portno;
    // thread_id = data[0];
    portno = data[1];

    while(true) {
        int one = 1;
        int sockfd, newsockfd;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;

        do {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
        }
        while(sockfd < 3);

        //server actually waits here for client to connect. Why?
        setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&one,sizeof(one));

        memset((char *) &serv_addr,0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
        serv_addr.sin_port = htons(portno);

        do {
            //printf("binding socket...\n");
        }while(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0);

        listen(sockfd,128);
        clilen = sizeof(cli_addr);

        while(true) {
            do {
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            }
            while(newsockfd<3);

            pthread_mutex_lock(&fd_mutex);
            while(((front+1) % BUFFER_SIZE) == rear)
            {
                //printf("master sleeping\n");
                pthread_cond_wait(&empty, &fd_mutex);//wait on empty as no slot is empty till some worker thread empties next slot
            }

            //add fd to fd_buffer
            fd_buffer[front] = newsockfd;
            //printf("master thread wrote fd %d at index %d\n",newsockfd,front);
            front = ( front + 1 ) % BUFFER_SIZE;

            pthread_cond_broadcast(&full);
            pthread_mutex_unlock(&fd_mutex);

        }
        close(sockfd);
    }
    return NULL;
}

void *process_conn(void *data) {
    // int thread_id = *((int *)data);
    int max_value_size;
    int newsockfd = 0;
    // int local_rear;
    while(true) {
        pthread_mutex_lock(&fd_mutex);
        while(rear == front) {
            //printf("thread %d sleeping\n",thread_id);
            pthread_cond_wait(&full, &fd_mutex);//wait on full as no slot is filled till master fills next slot
        }
        // local_rear = rear;
        newsockfd = fd_buffer[rear];
        fd_buffer[rear] = -1;
        rear = ( rear + 1 ) % BUFFER_SIZE;
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&fd_mutex);

        //process request
        if(newsockfd < 3)//invalid fd
            continue;
        //printf("thread %d read fd %d at index %d\n",thread_id, newsockfd, local_rear);

        char cmd_code[2];
        int n;
        while(true) {
            memset(cmd_code,0,sizeof(cmd_code));
            do {
                n = read(newsockfd,cmd_code,1);
            }while(n==0);
            if (n < 0) {
                break;
            } else if(n > 0) {
                if(strcmp(cmd_code,"5")==0) {
                    //disconnect code 5 received
                    for(long long i=0;i<COUNT;i++);
                    do {
                        n = write(newsockfd,"0",1);//send status code 0 (OK)
                    }while(n==0);
                    if(n < 0) {
                        // write_err();
                    }
                    break;
                } else if(strcmp(cmd_code,"0")==0) {
                    //connect code 0 received
                    for(long long i=0;i<COUNT;i++);
                    do {
                        n = write(newsockfd,"0",1);//send status code 0 (OK)
                    }while(n==0);
                    if (n < 0) {
                        // write_err();
                        break;
                    }
                } else if((strcmp(cmd_code,"1")==0)||(strcmp(cmd_code,"2")==0)) {
                    //create code 1 or update code 2 received
                    for(long long i=0;i<COUNT;i++);
                    int cmd, char_read;
                    char buf[2];
                    int key_len;
                    char key[64];
                    int value_size_len;
                    char value_size[8];
                    int size;
                    char buffer[256];

                    if(strcmp(cmd_code,"1")==0)
                        cmd = 1;//create command
                    else
                        cmd = 2;//update command

                    //receive key length
                    memset(buf,0,sizeof(buf));
                    do {
                        n = read(newsockfd,buf,1);
                    }while(n==0);
                    if (n < 0) {
                        break;
                    }
                    key_len = buf[0] - 32;

                    //receive key
                    char_read = 0;
                    memset(key,0,sizeof(key));
                    while(char_read < key_len) {
                        memset(buf,0,sizeof(buf));
                        n = read(newsockfd,buf,1);
                        if (n < 0) {
                            break;
                        }
                        char_read = char_read + n;
                        strcat(key,buf);
                    }
                    if(n < 0) {
                        break;
                    }

                    //receive value-size length
                    memset(buf,0,sizeof(buf));
                    do {
                        n = read(newsockfd,buf,1);
                    }while(n==0);
                    if (n < 0) {
                        break;
                    }
                    value_size_len = buf[0] - 32;

                    //receive value-size
                    char_read = 0;
                    memset(value_size,0,sizeof(value_size));
                    while(char_read < value_size_len) {
                        memset(buf,0,sizeof(buf));
                        n = read(newsockfd,buf,1);
                        if (n < 0) {
                            break;
                        }
                        char_read = char_read + n;
                        strcat(value_size,buf);
                    }
                    if (n < 0) {
                        break;
                    }
                    size = strtol(value_size, NULL, 10);
                    max_value_size = size + 2;                    

                    //receive value
                    char value[max_value_size];
                    char_read = 0;
                    memset(value,0,sizeof(value));
                    while(char_read < size) {
                        memset(buffer,0,sizeof(buffer));
                        n = read(newsockfd,buffer,sizeof(buffer)-1);
                        if (n < 0) {
                            break;
                        }
                        char_read = char_read + n;
                        strcat(value,buffer);
                    }
                    if (n < 0) {                        
                        break;
                    }                

                    int key_final = strtol(key, NULL, 10);//handle return value of strtol properly
                    int key_exists = 0;

                    pthread_mutex_lock(&kv_mutex);
                    auto it = key_value.find(key_final);
                    if(it == key_value.end()) {
                        key_exists = 0;
                    } else {
                        key_exists = 1;
                    }
                    if(cmd == 1) {
                        //create
                        if(key_exists == 0) {
                            key_value.insert(pair<int, string>(key_final, value));
                            do {
                                n = write(newsockfd,"0",1);//in create 0 means all OK
                            }while(n==0);
                            if (n < 0) {                                
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }
                        } else {
                            do {
                                n = write(newsockfd,"1",1);//in create 1 means key already exists
                            }while(n==0);
                            if (n < 0) {                                
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }
                        }
                    } else {
                        //update
                        if(key_exists == 0) {
                            do {
                                n = write(newsockfd,"1",1);//in update 1 means key doesn't exist
                            }while(n==0);
                            if (n < 0) {                        
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }
                        } else {
                            key_value.erase(key_final);
                            key_value.insert(pair<int, string>(key_final, value));
                            do {
                                n = write(newsockfd,"0",1);//in update 0 means all OK
                            }while(n==0);
                            if (n < 0) {
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }
                        }
                    }
                    /*for (auto it = key_value.begin(); it != key_value.end(); ++it)
                        cout << it->first << " = " << it->second << " "<< (it->second).length() <<endl;*/
                    pthread_mutex_unlock(&kv_mutex);
                } else if((strcmp(cmd_code,"3")==0)||(strcmp(cmd_code,"4")==0)) {
                    for(long long i=0;i<COUNT;i++);
                    int cmd;
                    char buf[2];
                    int key_len;
                    int char_read;
                    char key[64];

                    if(strcmp(cmd_code,"3")==0)
                        cmd = 3;
                    else
                        cmd = 4;

                    //receive key length
                    memset(buf,0,sizeof(buf));
                    do {
                        n = read(newsockfd,buf,1);
                    }while(n==0);
                    if (n < 0) {                        
                        break;
                    }
                    key_len = buf[0] - 32;

                    //receive key
                    char_read = 0;
                    memset(key,0,sizeof(key));
                    while(char_read < key_len) {
                        memset(buf,0,sizeof(buf));
                        n = read(newsockfd,buf,1);
                        if (n < 0) {
                            break;
                        }
                        char_read = char_read + n;
                        strcat(key,buf);
                    }
                    if (n < 0) {                        
                        break;
                    }

                    int key_final = strtol(key, NULL, 10);//handle return value of strtol properly

                    pthread_mutex_lock(&kv_mutex);
                    auto it = key_value.find(key_final);
                    if(it == key_value.end()) {
                        do {                    
                            n = write(newsockfd,"1",1);//1 means key doesn't exist
                        }while(n==0);
                        if (n < 0) {                            
                            pthread_mutex_unlock(&kv_mutex);
                            break;
                        }
                    } else {
                        do {                            
                            n = write(newsockfd,"0",1);//0 means key exists
                        }while(n==0);
                        if (n < 0) {                        
                            pthread_mutex_unlock(&kv_mutex);
                            break;
                        }

                        if(cmd == 3) {
                            //read
                            string read_str;
                            read_str = it -> second;
                            int size;
                            size = read_str.size();
                            max_value_size = size + 2;
                            char read_value[max_value_size];

                            int value_size_len;

                            char size_as_str[64];
                            char buffer[256];
                            int char_written;

                            strcpy(read_value,read_str.c_str());

                            sprintf(size_as_str, "%d", size);

                            //send value-size length
                            value_size_len = 32 + strlen(size_as_str);
                            /*to get into printable char range
                            max "value-size" string length is 63 to avoid overflow
                            actual max "value-size" string length is just 5
                            coz "value" string can be max of 4096 characters*/
                            memset(buf,0,sizeof(buf));
                            sprintf(buf, "%c", value_size_len);
                            do {
                                n = write(newsockfd,buf,1);
                            }while(n==0);
                            if (n < 0) {
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }

                            //send value-size
                            char_written = 0;
                            int size_as_str_len = strlen(size_as_str);
                            while(char_written < size_as_str_len) {
                                n = write(newsockfd,size_as_str + char_written, size_as_str_len - char_written);
                                if (n < 0) {
                                    break;
                                }
                                char_written = char_written + n;
                            }
                            if(n < 0) {
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }

                            //send value
                            char_written = 0;
                            while(char_written < size) {
                                memset(buffer,0,sizeof(buffer));
                                strncpy(buffer,read_value+char_written,sizeof(buffer)-1);
                                n = write(newsockfd,buffer,strlen(buffer));
                                if(n < 0) {
                                    break;
                                }
                                char_written = char_written + n;
                            }
                            if(n < 0) {
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }

                            memset(buf,0,sizeof(buf));
                            do {
                                n = read(newsockfd,buf,1);
                            }while(n==0);
                            if(n < 0) {
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }

                            if(strcmp(buf,"0")==0) {
                                //0 means value read by client
                            } else {
                                //some error msg
                            }
                        } else {
                            //delete
                            key_value.erase(key_final);
                            do {
                                n = write(newsockfd,"0",1);//0 means all OK
                            }while(n==0);
                            if (n < 0) {                                
                                pthread_mutex_unlock(&kv_mutex);
                                break;
                            }
                        }
                    }
                    /*for (auto it = key_value.begin(); it != key_value.end(); ++it)
                        cout << it->first << " = " << it->second << " "<< (it->second).length() <<endl;*/
                    pthread_mutex_unlock(&kv_mutex);
                }
            } else {
                //n == 0, 0 bytes read from socket
                //this part is never executed
            }
        }
        close(newsockfd);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int thread_id_port[2];
    thread_id_port[0]=0;
    int workers = 4;//default
    if(argc!=3 && argc!=4)
    {
        printf("Usage: %s <server-ip> <server-port-no> [#workers]\n",argv[0]);
        return -1;
    }
    if(argc==4)
    {
        workers = strtol(argv[3], NULL, 10);
    }

    thread_id_port[1]=strtol(argv[2], NULL, 10);//check return value of strtol

    serv_ip = argv[1];

    int i;
    pthread_t prod_thread;
    front = 0;
    rear = BUFFER_SIZE-1;

    pthread_mutex_init(&kv_mutex, NULL);
    pthread_mutex_init(&fd_mutex, NULL);
    pthread_cond_init(&empty, NULL);
    pthread_cond_init(&full, NULL);

    //create master thread
    pthread_create(&prod_thread, NULL, accept_conn, (void *)&thread_id_port);

    //create WORKERS worker threads

    int worker_x_id[workers];
    pthread_t worker_x[workers];
    for(i = 0;i < workers;i++)
    {
        worker_x_id[i] = i + 1;
        pthread_create(&worker_x[i], NULL, process_conn, (void *)&worker_x_id[i]);
    }

    pthread_join(prod_thread, NULL);
    for(i = 0;i < workers;i++)
    {
        pthread_join(worker_x[i], NULL);
    }
    return 0;
}
