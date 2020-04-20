/*
    Client/Load-generator Application for CMS
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <math.h>

#include <limits>
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

#define KEY_LIMIT 1000000000
#define MAX_SERVER_COUNT 10

pthread_mutex_t conn_mutex, req_mutex, serv_ip_mutex;
long total_req = 0, total_res = 0;
double totalTime = 0;

char serv_ip[MAX_SERVER_COUNT][20];
int serv_count = 0;
int serv_port = 55555;
// char mon_ip[20];
// int mon_port = 55554;

void *run_monitor_thread(void* monitor_argv){
    // IPC between loadgenerator and monitor program is done using file for simplicity
    // other IPC mechanism can also be used
    while(true){
        string fileName = "./activeDomainInfo.txt";
        ifstream f;
        f.open(fileName.c_str());
        if(!f){
            // printf("File '%s' does not exist...\n", fileName.c_str());
            sleep(1);
            continue;
        }
        string str;
        int s_count = 0;
        pthread_mutex_lock(&serv_ip_mutex);
        while (getline(f, str)) {
            strcpy(serv_ip[s_count],str.c_str());
            s_count++;
        }
        f.close();
        serv_count = s_count;
        pthread_mutex_unlock(&serv_ip_mutex);
        sleep(1);
    }
}

int gen_rand_no(int lower, int upper) {
    return ((rand() % (upper - lower + 1)) + lower);
}

int connect_to_server(int *sockfd) {
////////////////////////////////////////////  mutexed code to handle seg fault ///////////////////////////////////////////////
    pthread_mutex_lock(&conn_mutex);
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[2];
    int n, port_no;

    do {
        *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    } while(*sockfd < 3);
    server = NULL;

    //////////////////// mutex to read from serv_ip array/////////////////////////////
    pthread_mutex_lock(&serv_ip_mutex);
    do {
        int index = gen_rand_no(0, serv_count - 1);
        server = gethostbyname(serv_ip[index]);
    } while(server == NULL);
    pthread_mutex_unlock(&serv_ip_mutex);
    //////////////////////////////////////////////////////////////////////////////////

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    /*seg faults for multiple threads without mutex on CS if running on multiple cores
    however works with multiple threads on single core. why?*/
    port_no = serv_port;
    serv_addr.sin_port = htons(port_no);
    do {
    } while(connect(*sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0);
    pthread_mutex_unlock(&conn_mutex);
////////////////////////////////////////////////////////////////////////////////////////////////////////
    do {
        n = write(*sockfd,"0",1);//send connect code 0
    } while(n==0);
    if(n < 0){
        return -1;
    }
    memset(buffer,0, sizeof(buffer));
    do {
        n = read(*sockfd,buffer,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }
    if(strcmp(buffer,"0")==0){
        //status code 0 received (OK)
    	return 0;
    } else {
    	//error msg: received code is not 0
    	return -1;
    }
}

int disconnect(int *sockfd) {
    int n;
    char buffer[2];
    do {
        n = write(*sockfd,"5",1);//send disconnect code 5
    } while(n==0);
    if(n < 0){
        return -1;
    }
    memset(buffer,0, sizeof(buffer));
    do {
        n = read(*sockfd,buffer,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }
    if(strcmp(buffer,"0")==0){
        //status code 0 received (OK)
    	if(close(*sockfd)==0){
        	return 0;
    	} else {
    		//error msg: cannot close socket
        	return -1;
    	}
   	} else {
   		//error msg: received code is not 0
   		return -1;
   	}
}

int create_update(int *sockfd, int cmd) {
    char buf[2];
    int n;
    // int value_len;
    int key_len;
    int value_size_len;
    int size;
    int char_written;
    char buffer[4];

    char set_key_str[16];
    int set_key = gen_rand_no(1, KEY_LIMIT);
    sprintf(set_key_str, "%d", set_key);

    ///////////////////////////
    char set_value_size_str[16];
    int len = strlen(set_key_str);
    sprintf(set_value_size_str, "%d", len);

    ///////////////////////////
    char set_value[strlen(set_key_str)+1];
    memset(set_value, 0, sizeof(set_value));
    strcpy(set_value,set_key_str);

    char param[4][16];
    strcpy(param[1],set_key_str);
    strcpy(param[2],set_value_size_str);
    strcpy(param[3],set_value);

    memset(buf,0,sizeof(buf));
    if(cmd == 1){
    	buf[0] = '1';
        strcpy(param[0],"create");
    } else {
    	buf[0] = '2';
        strcpy(param[0],"update");
    }
    //send create/update code
    do {
        n = write(*sockfd,buf,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }

    //send key length
    key_len = 32 + strlen(param[1]);
    /*to get into printable char range
    max key length is 63 to avoid overflow*/
    sprintf(buf, "%c", key_len);
    do {
        n = write(*sockfd,buf,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }

    //send key
    char_written = 0;
    key_len = key_len - 32;//resetting it to actual value
    while(char_written < key_len){
        n = write(*sockfd, param[1] + char_written, key_len - char_written);
        if(n < 0){
            break;
        }
        char_written = char_written + n;
    }
    if(n < 0){
        return -1;
    }

    //send value-size length
    value_size_len = 32 + strlen(param[2]);
    /*to get into printable char range
    max "value-size" string length is 63 to avoid overflow
	actual max "value-size" string length is just 5
	coz "value" string can be max of 4096 characters*/

    sprintf(buf, "%c", value_size_len);
    do {
        n = write(*sockfd,buf,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }

    //send value-size
    char_written = 0;
    value_size_len = value_size_len -32;//resetting it to actual value
    while(char_written < value_size_len){
        n = write(*sockfd,param[2] + char_written, value_size_len - char_written);
        if (n < 0){
            break;
        }
        char_written = char_written + n;
    }
    if(n < 0){
        return -1;
    }

    //send value
    char_written = 0;
    size = strlen(param[3]);
    while(char_written < size){
    	memset(buffer,0,sizeof(buffer));
    	strncpy(buffer,param[3]+char_written,sizeof(buffer)-1);
        n = write(*sockfd,buffer,strlen(buffer));
        if(n < 0){
            break;
        }
        char_written = char_written + n;
    }
    if(n < 0){
        return -1;
    }
    memset(buf,0, sizeof(buf));
    do {
        n = read(*sockfd,buf,1);
    } while(n==0);
    if(n < 0){
        return -1;
    }

    if(strcmp(param[0],"create")==0){
    	if(strcmp(buf,"0")==0){
            //status code 0 received (OK)
    		//printf("create: OK\n");
    	} else {
    		//printf("create: key already exists\n");
		}
	} else {
		if(strcmp(buf,"0")==0){
            //status code 0 received (OK)
    		//printf("update: OK\n");
    	} else {
    		//printf("update: key doesn't exist\n");
		}
	}
	return 0;
}

int read_delete(int *sockfd, int cmd){
    char buf[2];
    int key_len, n, char_written;
    char param[4][16];

    memset(buf,0,sizeof(buf));
    if(cmd==3){
    	buf[0] = '3';
        strcpy(param[0],"read");
    } else {
    	buf[0] = '4';
        strcpy(param[0],"delete");
    }

    char set_key_str[16];
    int set_key = gen_rand_no(1, KEY_LIMIT);
    sprintf(set_key_str, "%d", set_key);

    strcpy(param[1],set_key_str);

    //send read/delete code
    do {
        n = write(*sockfd,buf,1);
    } while(n==0);
    if(n < 0) {
        return -1;
    }

    //send key length
    key_len = 32 + strlen(param[1]);
    /*to get into printable char range
    max key length is 63 to avoid overflow*/
    sprintf(buf, "%c", key_len);
    do {
        n = write(*sockfd,buf,1);
    } while(n==0);
    if(n < 0) {
        return -1;
    }

    //send key
    char_written = 0;
    key_len = key_len - 32;//resetting it to actual value
    while(char_written < key_len) {
        n = write(*sockfd,param[1] + char_written, key_len - char_written);
        if(n < 0) {
            break;
        }
        char_written = char_written + n;
    }
    if(n < 0) {
        return -1;
    }

    //receive exist status
    memset(buf,0, sizeof(buf));
    do {
        n = read(*sockfd,buf,1);
    } while(n==0);
    if(n < 0) {
        return -1;
    }

    if(strcmp(buf,"0")==0) {
        //status code 0 received (OK) key exists
    	if(strcmp(param[0],"read")==0) {
    		int value_size_len;
    		int char_read;
    		char value_size[8];
    		int size;

    		char buffer[256];

    		//receive value-size length
            memset(buf,0,sizeof(buf));
            do {
                n = read(*sockfd,buf,1);
            } while(n==0);
            if(n < 0) {
                return -1;
            }
            value_size_len = buf[0] - 32;

            //receive value-size
            char_read = 0;
            memset(value_size,0,sizeof(value_size));
            while(char_read < value_size_len) {
                memset(buf,0,sizeof(buf));
                n = read(*sockfd,buf,1);
                if(n < 0) {
                    break;
                }
                char_read = char_read + n;
                strcat(value_size,buf);
            }
            if(n < 0)
            {
                return -1;
            }
            size = strtol(value_size, NULL, 10);

            //receive value
            char value[size+1];
            char_read = 0;
            memset(value,0,sizeof(value));
            while(char_read < size) {
                memset(buffer,0,sizeof(buffer));
                n = read(*sockfd,buffer,sizeof(buffer)-1);
                if(n < 0) {
                    break;
                }
                char_read = char_read + n;
                strcat(value,buffer);
            }
            if(n < 0) {
                return -1;
            }

            do {
                n = write(*sockfd,"0",1);
            } while(n==0);
            if(n < 0) {
    
                return -1;
            }
    	} else {
    		memset(buf,0,sizeof(buf));
            do {
                n = read(*sockfd,buf,1);
            } while(n==0);
		    if(n < 0) {
		        return -1;
		    }
		    if(strcmp(buf,"0")==0) {
		    	
		    } else {
		    	//error msg: received code is not 0
   				return -1;
		    }
    	}
    } else {
	}
	return 0;
}

void *run_worker_thread(void * worker_argv){
    int *data = (int *)worker_argv;
    int time_in_sec = data[0];
    // int thread_id = data[1];

    long            ns; // Nanoseconds
    time_t          cur_time_s;  // Seconds
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    cur_time_s  = spec.tv_sec;
    ns = spec.tv_nsec;

    srand((unsigned int)ns);//seed rand function

    int cmd, cmd_status;
    int connected = 0;
    int sockfd;

    clock_t t1, t2;

    int lower = 1, upper = 4;
    long req_count = 0, res_count = 0;

    t1 = clock();
    do {
        cmd = gen_rand_no(lower, upper);
        if(connected == 0){
            req_count++;
            cmd_status = connect_to_server(&sockfd);
            if(cmd_status == 0){
                res_count++;
                connected = 1;
            } else {
                continue;
            }
        }
        switch(cmd)
        {
            //create
            case 1:     req_count++;
                        cmd_status = create_update(&sockfd, cmd);
                        if(cmd_status == 0){
                            res_count++;
                        }
                        break;

            //update
            case 2:     req_count++;
                        cmd_status = create_update(&sockfd, cmd);
                        if(cmd_status == 0){
                            res_count++;
                        }
                        break;

            //read
            case 3:     req_count++;
                        cmd_status = read_delete(&sockfd, cmd);
                        if(cmd_status == 0){
                            res_count++;
                        }
                        break;

            //delete
            case 4:     req_count++;
                        cmd_status = read_delete(&sockfd, cmd);
                        if(cmd_status == 0){
                            res_count++;
                        }
                        break;

            default: printf("Invalid command\n");
            //this should never print
        }
        if(connected == 1){
            req_count++;
            cmd_status = disconnect(&sockfd);
            if(cmd_status == 0){
                res_count++;
                connected = 0;
            } else {
                continue;
            }
        }
        clock_gettime(CLOCK_REALTIME, &spec);
    } while((int)(spec.tv_sec - cur_time_s) < time_in_sec);

    t2 = clock();

    pthread_mutex_lock(&req_mutex);
    total_req = total_req + req_count;
    total_res = total_res + res_count;
    totalTime = totalTime + (((double)(t2 - t1))/CLOCKS_PER_SEC)/res_count;
    pthread_mutex_unlock(&req_mutex);
    return NULL;
}

void run_mode(int time_in_sec, int no_workers, ofstream& logFile){
    
        pthread_mutex_init(&conn_mutex, NULL);
        pthread_mutex_init(&req_mutex, NULL);

        int i;
        int thread_argv[no_workers][2];//index 0: time in sec, index 1: worker id,
        for (i = 0; i < no_workers; i++){
            thread_argv[i][0] = time_in_sec;
        }

        //create no_workers worker threads
        int worker_x_id[no_workers];
        pthread_t worker_x[no_workers];
        for(int i=0;i<no_workers;i++){
            worker_x_id[i] = i + 1;
            thread_argv[i][1] = worker_x_id[i];
            pthread_create(&worker_x[i], NULL, run_worker_thread, (void *)&thread_argv[i]);
        }

        for(i=0;i<no_workers;i++){
            pthread_join(worker_x[i], NULL);
        }
        logFile << no_workers << ",";
        logFile << ((float)total_req)/time_in_sec << ",";
        logFile << ((float)totalTime)/no_workers << "\n";

        total_req = 0;
        totalTime = 0;
}

int main(int argc, char *argv[])
{
    int min_threads, max_threads;
    int time_in_sec;
    int mode = 0;
    if(argc!=6){
        printf("Usage: %s <default_server_ip> <monitor_ip> <min_threads> <max_threads> <test_time_in_secs|time_per_thread_in_secs>\n",argv[0]);
        printf("Supports two modes:\n");
        printf("1. Constant mode\n");
        printf("Give <min_threads> equal to <max_threads>\n");
        printf("Specify <test_time_in_secs>\n");
        printf("Ex. %s 192.168.122.2 127.0.0.1 30 30 25\n", argv[0]);
        printf("2. Sawtooth mode\n");
        printf("Test time will be equal to (2*(<max_threads> - <min_threads>) + 1)*<time_per_thread_in_secs> seconds\n");
        printf("Specify <time_per_thread_in_secs>\n");
        printf("Ex. %s 192.168.122.2 127.0.0.1 20 60 2\n", argv[0]);
        return -1;
    }
    strcpy(serv_ip[0], argv[1]);
    serv_count++;

    ofstream file("./activeDomainInfo.txt");
    if ( file.is_open() ){
        // do nothing    
        // logFile << "threadsAtClient,requestsPerSec,responseTime\n";
        file << serv_ip[0]<<"\n";
        file.close();
    } else {
        cout << "File 'activeDomainInfo.txt' failed to open"; 
    }

    // strcpy(mon_ip, argv[2]);
    if(atoi(argv[3]) <= 0){
        //*atoi function does not detect errors, use strtol(param[1], NULL, 10) instead*/
        printf("<min_threads> must be a positive integer\n");
        return -1;
    } else {
        min_threads = atoi(argv[3]);
    }
    if(atoi(argv[4]) <= 0){
        /*atoi function does not detect errors, use strtol(param[1], NULL, 10) instead*/
        printf("<max_threads> must be a positive integer\n");
        return -1;
    } else {
        max_threads = atoi(argv[4]);
    }

    if(max_threads < min_threads){
        printf("<max_threads> cannot be less than <min_threads>\n");
        return -1;
    }

    if(atoi(argv[5]) <= 0){
        /*atoi function does not detect errors, use strtol(param[1], NULL, 10) instead*/
        printf("<test_time_in_secs|time_per_thread_in_secs> must be a positive integer\n");
        return -1;
    } else {
        time_in_sec = atoi(argv[5]);
    }
    if(max_threads == min_threads){
        mode = 1;
    } else {
        mode = 2;
    }
    // if(mode == 1 && time_in_sec < 20){
    //     printf("For Constant mode <test_time_in_secs> must be >=20\n");
    //     return -1;    
    // }
    // if(mode == 2 && time_in_sec > 5){
    //     printf("For Sawtooth mode <time_per_thread_in_secs> must be <=5\n");
    //     return -1;
    // }

    pthread_mutex_init(&serv_ip_mutex, NULL);
    pthread_t monitor_thread;
    int monitor_argv;
    pthread_create(&monitor_thread, NULL, run_monitor_thread, (void *)&monitor_argv);


    // log stats
    ofstream logFile("./log.txt");
    if ( logFile.is_open() ){
        // do nothing    
        // logFile << "threadsAtClient,requestsPerSec,responseTime\n";
    } else {
        cout << "Log file 'log.txt' failed to open"; 
    }

    if(mode == 1){
        int test_time = 0;
        while(test_time < time_in_sec){
            run_mode(1, min_threads, logFile);
            test_time += 1;
        }
    } else if(mode == 2){
        int no_workers = min_threads;
        while(no_workers <= max_threads){
            run_mode(time_in_sec, no_workers, logFile);
            no_workers++;
        }
        no_workers = max_threads - 1;
        while(no_workers >= min_threads){
            run_mode(time_in_sec, no_workers, logFile);
            no_workers--;
        }
    } 

    logFile.close(); // Closing the log file

    return 0;  
}
