/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *       |<-  4 byte  ->|<-  2 byte  ->|<-  2 byte  ->|
 *       | sequence_num |  ack         |  checksum    |
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <unordered_map>
#include "rdt_struct.h"
#include "rdt_receiver.h"
#include <string>


const int window = 10;
const int sizeof_sequence_num = 4;
const int sizeof_ack = 2;
const int sizeof_checksum = 2;
const int sizeof_payload_size=1;
/* 1-byte header indicating the size of the payload */
const int header_size = sizeof_sequence_num + sizeof_checksum + sizeof_payload_size;
/* position pointer of each */
const int pointer_sequence_num = 0;
const int pointer_check = 126;
const int pointer_payload_size = sizeof_sequence_num;
const int pointer_ack = sizeof_sequence_num ;
const int pointer_payload = sizeof_sequence_num  + sizeof_payload_size;
const double timeout = 0.3;
unsigned int receiver_next_seq_num = 0;

std::unordered_map<unsigned int, packet*> receiver_buffer;
/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

uint16_t checksum2(struct packet *pkt)
{
    unsigned long checksum = 0; 
    
    for (int i = 0; i < 126; i += 2) {
        checksum += *(short *)(&(pkt->data[i]));
    }
    while (checksum >> 16) {
        checksum = (checksum >> 16) + (checksum & 0xffff);
    }
    return ~checksum;
}
uint16_t checksum4(struct packet *pkt)
{
    unsigned long checksum = 0; 
    
    for (int i = 0; i < 6; i += 2) {
        checksum += *(short *)(&(pkt->data[i]));
    }
    while (checksum >> 16) {
        checksum = (checksum >> 16) + (checksum & 0xffff);
    }
    return ~checksum;
}

void send_ack(unsigned int ack_num, short ack) {
    struct packet* ack_pkt = (struct packet*) malloc(sizeof(struct packet));

    memcpy(ack_pkt->data, &ack_num, sizeof_sequence_num);
    memcpy(ack_pkt->data + 4, &ack, sizeof_ack);

    uint16_t che=checksum4(ack_pkt);
    memcpy(ack_pkt->data + 6, &che, sizeof_checksum);
    Receiver_ToLowerLayer(ack_pkt); 
};


/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{

    unsigned int seq_num = 0;
    short payload_size = 0;
    uint16_t che = 0;
    memcpy(&seq_num, pkt->data, sizeof_sequence_num);
    memcpy(&payload_size, pkt->data + 4, sizeof_payload_size);
    memcpy(&che, pkt->data + 126, sizeof_checksum);
    // /* 1-byte header indicating the size of the payload */
    // int header_size = 1;

    // /* construct a message and deliver to the upper layer */
    // struct message *msg = (struct message*) malloc(sizeof(struct message));
    //这里对于坏包的情况做检查，防止段错误
    if (payload_size < 0) payload_size = 0;
    if (payload_size > RDT_PKTSIZE - header_size) payload_size = RDT_PKTSIZE - header_size;
    std::string data(pkt->data + pointer_payload, pkt->data + pointer_payload + payload_size);
    uint16_t check=checksum2(pkt);
    //std::cout<<che<<"  "<<check<<"       "<<seq_num<<"  "<<receiver_next_seq_num<<std::endl;
    if(che==check  && seq_num == receiver_next_seq_num){
      //fprintf(stdout, "At %.2fs: aaaaa ...\n", GetSimulationTime());
      //fprintf(stdout, "At %.2fs: bbbbb ...\n", GetSimulationTime());
        receiver_buffer[seq_num] = pkt;

        //这里是
        while (receiver_buffer.find(receiver_next_seq_num) != receiver_buffer.end()) {          
        packet* more_pkt = receiver_buffer[receiver_next_seq_num]; 
        ASSERT(more_pkt != NULL);
        unsigned int more_seq_num = 0;
        memcpy(&more_seq_num, more_pkt->data + pointer_sequence_num, sizeof_sequence_num); 
        ASSERT(more_seq_num == receiver_next_seq_num);
        short more_che = 0;
        memcpy(&more_che, more_pkt->data + pointer_check, sizeof_checksum);
        char more_payload_size = 0;
        memcpy(&more_payload_size, more_pkt->data + pointer_payload_size, sizeof_payload_size); 

        struct message *more_msg = (struct message*) malloc(sizeof(struct message));
        ASSERT(more_msg != NULL);
        more_msg->size = more_payload_size;

        more_msg->data = (char*) malloc(more_msg->size);
        ASSERT(more_msg->data != NULL);
        memcpy(more_msg->data, more_pkt->data + pointer_payload, more_payload_size);
        Receiver_ToUpperLayer(more_msg); // printf("Receiver: send packet[%d] to upper layer\n", more_seq_num);
        /* don't forget to free the space */
        if (more_msg->data != NULL) free(more_msg->data);
        if (more_msg != NULL) free(more_msg);

        receiver_next_seq_num++;
      };

    /* send back ACK */
    send_ack(receiver_next_seq_num - 1, 1);
    
    } 
    else if (che == check && receiver_next_seq_num < seq_num) { // buffer it
      packet* buffer_pkt = (struct packet*) malloc(sizeof(struct packet));
      ASSERT(buffer_pkt);
      memcpy(buffer_pkt->data, pkt->data, sizeof(pkt->data));
      receiver_buffer[seq_num] = buffer_pkt;

      /* send back NACK */
      send_ack(receiver_next_seq_num, 0);
      //fprintf(stdout, "At %.2fs: bbbbb ...\n", GetSimulationTime());
    }
    else send_ack(receiver_next_seq_num - 1, 1);
    











    // msg->size = pkt->data[0];

    // /* sanity check in case the packet is corrupted */
    // if (msg->size<0) msg->size=0;
    // if (msg->size>RDT_PKTSIZE-header_size) msg->size=RDT_PKTSIZE-header_size;

    // msg->data = (char*) malloc(msg->size);
    // ASSERT(msg->data!=NULL);
    // memcpy(msg->data, pkt->data+header_size, msg->size);
    // Receiver_ToUpperLayer(msg);

    // /* don't forget to free the space */
    // if (msg->data!=NULL) free(msg->data);
    // if (msg!=NULL) free(msg);
}
