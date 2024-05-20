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



#include <queue>

using namespace std;

#define MSS                 4000
#define MAX_QUEUE_SIZE      1000

enum type_t {
    DATA,
    ACK,
    FIN,
    FINACK
};

typedef struct {
	int size;
	type_t type;
	uint64_t ack_num;
    uint64_t seq_num;
	char data[MSS];
} packet_t;

struct compare {
    bool operator()(packet_t greater, packet_t less) {
        return greater.seq_num > less.seq_num;
    }
};

priority_queue<packet_t, vector<packet_t>, compare> buf;



struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}



void send_packet(uint64_t i, type_t type) {
    packet_t packet;
    packet.size = 0;
    packet.type = type;
    packet.ack_num = i;
    packet.seq_num = 0;
    memset(packet.data, 0, MSS);

    if(sendto(s, &packet, sizeof(packet_t), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other)) == -1)
        diep("sendto()");
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    FILE* file;
    uint64_t i = 0;
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */ 
    file = fopen(destinationFile, "wb");
    if(file == NULL) {
        printf("Could not open file.");
        exit(1);
    }

    while(1) {
        packet_t packet;
        if(recvfrom(s, &packet, sizeof(packet_t), 0, (sockaddr*)&si_other, (socklen_t*)&slen) == -1)
            diep("recvfrom()");
       
        if(packet.type == DATA) {
            if(i == packet.seq_num) {
                fwrite(packet.data, sizeof(char), packet.size, file);
                i += packet.size;

                while(!buf.empty() && i == buf.top().seq_num) {
                    packet_t prev = buf.top();
                    buf.pop();

                    fwrite(prev.data, sizeof(char), prev.size, file);
                    i += packet.size;
                }
            }
            else if(i < packet.seq_num) {
                if(buf.size() < MAX_QUEUE_SIZE)
                    buf.push(packet);
            }

            send_packet(i, ACK);
        } 
        else if(packet.type == FIN) {
            send_packet(i, FINACK);
            break;
        }
    }

    fclose(file);
    close(s);
		printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

