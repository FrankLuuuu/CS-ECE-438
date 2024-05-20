#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>



#include <math.h>
#include <queue>
#include <iostream>

using namespace std;

#define MSS                 4000
#define MAX_QUEUE_SIZE      1000
#define TIMEOUT             100000
#define INITITAL_SSTHRESH   512 

enum type_t {
    DATA,
    ACK,
    FIN,
    FINACK
};

enum status_t {
    SLOW_START = 20,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
};

typedef struct {
	int size;
	type_t type;
	uint64_t ack_num;
    uint64_t seq_num;
	char data[MSS];
} packet_t;

queue<packet_t> sent;
queue<packet_t> unsent;
float cwnd;
float ssthresh;
uint64_t seq;
uint64_t ACKnum;
uint64_t packet_count;
uint64_t duplicate_count;
uint64_t sent_count;
uint64_t bytes_left;
status_t status;    
FILE* file;



struct sockaddr_in si_other;
int s, slen;

void diep(const char *s) {
    perror(s);
    exit(1);
}



void send_single_packet(packet_t* packet) {
    if(sendto(s, packet, sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other)) == -1)
        diep("sendto()");
}

void load_packets() {
    int i, byte_count;
    char buf[MSS];
    memset(buf, 0, MSS);

    if(!bytes_left)
        return;
        
    for(i = 0; i < ceil((cwnd - sent.size() * MSS) / MSS); i++) {
        byte_count = fread(buf, sizeof(char), min(bytes_left, (uint64_t)MSS), file);

        if(byte_count > 0) {
            packet_t packet;
            packet.size = byte_count;
            packet.type = DATA;
            packet.seq_num = seq;
            memcpy(packet.data, &buf, byte_count);
            sent.push(packet);
            unsent.push(packet);

            seq += byte_count;
            bytes_left -= byte_count;
        }
    }
    
    while(!unsent.empty()) {
        packet_t packet = unsent.front();
        unsent.pop();
        send_single_packet(&packet);
        sent_count++;
    }
}

void timeout_rcvd() {
    status = SLOW_START;
    ssthresh = max((float)MSS * INITITAL_SSTHRESH / 2, cwnd / 2);
    cwnd = MSS * INITITAL_SSTHRESH / 2;
    duplicate_count = 0;

    queue<packet_t> resend = sent; 
    for(int i = 0; i < 32; i++) {
        if(resend.empty())
            continue;
        packet_t packet = resend.front();
        resend.pop();
        send_single_packet(&packet);
    }
}

void duplicate_ack_rcvd() {
    status = FAST_RECOVERY;
    ssthresh = max((float)MSS, cwnd / 2);
    cwnd = max((float)MSS, ssthresh + 3 * MSS);
    duplicate_count = 0;

    if(!sent.empty()) {
        packet_t packet = sent.front();
        send_single_packet(&packet);
    }
}

void status_update() {
    if(status == SLOW_START) {
        if (duplicate_count >= 3)
            duplicate_ack_rcvd();
        else if(duplicate_count == 0) {
            if(cwnd >= ssthresh) {
                status = CONGESTION_AVOIDANCE;
                return;
            }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
            cwnd += MSS;
            cwnd = max((float)MSS, cwnd);
        }
    }
    else if(status == CONGESTION_AVOIDANCE) {
        if(duplicate_count == 0) {
            cwnd += floor(1.0 * MSS / cwnd) * MSS; 
            cwnd = max((float)MSS, cwnd);
        }
        else if(duplicate_count >= 3) 
            duplicate_ack_rcvd();
    }
    else if(status == FAST_RECOVERY) {
        if(duplicate_count > 0) 
            cwnd += MSS;
        else if(duplicate_count == 0) {
            cwnd = max((float)MSS, ssthresh);
            status = CONGESTION_AVOIDANCE;
        }
    }
}

void ack_rcvd(packet_t* packet){
    int i, count;

    if (packet->ack_num == sent.front().seq_num) {
        duplicate_count++;
        status_update();
    }
    else if(packet->ack_num > sent.front().seq_num) {
        duplicate_count = 0;
        status_update();

        count = ceil((packet->ack_num - sent.front().seq_num) / (1.0 * MSS));
        ACKnum += count;

        for(i = 0; !sent.empty() && i < count; i++)
            sent.pop();

        load_packets();
    }
}

void close_connection() {
    char buf[sizeof(packet_t)];
    slen = sizeof(si_other);

    packet_t packet;
    packet.size = 0;
    packet.type = FIN;
    memset(packet.data, 0, MSS);
    send_single_packet(&packet);

    while(1) {
        if(recvfrom(s, buf, sizeof(packet_t), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen) != -1) {
            packet_t received;
            memcpy(&received, buf, sizeof(packet_t));

            if(received.type == FINACK) {
                packet.size = 0;
                packet.type = FINACK;
                send_single_packet(&packet);
                break;
            }
        }
        else {
            if(errno != EAGAIN || errno != EWOULDBLOCK)
                diep("recvfrom()");
            else {
                packet.size = 0;
                packet.type = FIN;
                memset(packet.data, 0, MSS);
                send_single_packet(&packet);
            }
        }
    }
}

void initialize_settings(unsigned long long int bytesToTransfer) {
    cwnd = MSS;
    ssthresh = cwnd * float(INITITAL_SSTHRESH);
    seq = 0;
    ACKnum = 0;
    packet_count = ceil(bytesToTransfer * 1.0 / MSS);
    duplicate_count = 0;
    bytes_left = bytesToTransfer;
    status = SLOW_START;
}



void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {


	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }    
    
    file = fopen(filename, "rb");
    if(file == NULL) {
        printf("Could not open file.");
        exit(1);
    }

    initialize_settings(bytesToTransfer);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
        diep("setsockopt");

	/* Send data and receive acknowledgements on s*/
    load_packets();
    while(sent_count < packet_count || ACKnum < packet_count) {
        packet_t packet;
        if((recvfrom(s, &packet, sizeof(packet_t), 0, NULL, NULL)) == -1) {
            if(errno != EAGAIN || errno != EWOULDBLOCK) 
                diep("recvfrom()");
            if(!sent.empty())
                timeout_rcvd();
        }
        else {
            if(packet.type == ACK)
                ack_rcvd(&packet);
        }
    }

    fclose(file);
    printf("Closing the socket\n");
    close_connection();
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


