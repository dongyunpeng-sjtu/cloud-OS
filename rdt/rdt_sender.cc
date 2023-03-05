/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  4 byte  ->|<-  1 byte  ->|<-   121 byte            ->|<-  2 byte  ->|
 *       | sequence_num | payload size |<-   payload             ->| checksum     |
 *
 *       The initial 4 bytes are seq_num
 *       Then is payload size of 1 byte
 *       Then is payload
 *       Finally is checksum 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque>
#include "rdt_struct.h"
#include "rdt_sender.h"
#include <string>

const int window =10;
const int sizeof_sequence_num = 4;
const int sizeof_payload_size = 1;
const int  sizeof_checksum  = 2;
const int  sizeof_ack = 2;
const int  payload_size_offset =4;
const int  payload_offset=5;
unsigned int sender_base = 0;
unsigned int sender_next_seq_num = 0;
const double timeout = 0.3;
const int ack_offset=4;
const int che_offset=6;
std::deque<packet> sender_window = std::deque<packet>();
/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    sender_base = 0;
    sender_next_seq_num = 0;
    sender_window.clear();
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

uint16_t checksum(struct packet *pkt)
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
uint16_t checksum3(struct packet *pkt)
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



/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 1-byte header indicating the size of the payload */
    int header_size = sizeof_payload_size+sizeof_sequence_num+ sizeof_checksum ;

    /* maximum payload size */
    int maxpayload_size = 121;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message *///这里是指message中因为split没有寄出的byte
    int cursor = 0;
    //fprintf(stdout, "At %.2fs: aaaaa ...\n", GetSimulationTime());
    //如果装不下去，就一直循环拆分
    while (msg->size > cursor)
    {
        char payload_size = std::min(maxpayload_size, msg->size - cursor);
        // void *memcpy(void *str1, const void *str2, size_t n) 从存储区 str2 复制 n 个字节到存储区 str1
        //这一步是复制payloadsize,是否有必要？
        memcpy(pkt.data + payload_size_offset, &payload_size, 1);
        //这一步复制整个payload
        memcpy(pkt.data + payload_offset, msg->data + cursor, payload_size);

        unsigned int seq_num = sender_window.size();
        memcpy(pkt.data , &seq_num, sizeof_sequence_num);

        


        uint16_t check= checksum(&pkt);
        memcpy(pkt.data + 126, &check, sizeof_checksum);

        /* throw it into sender_window */
        sender_window.push_back(pkt);

        printf("Sender: packet[%d] size %d\n", seq_num, payload_size);

        /* move the cursor */
        cursor += maxpayload_size;
    }
    //fprintf(stdout, "At %.2fs: bbbbb ...\n", GetSimulationTime());
    /* send first N packet */
    while (sender_next_seq_num < sender_base + window && sender_next_seq_num < sender_window.size()) {
        Sender_ToLowerLayer(&sender_window[sender_next_seq_num]); 
        printf("Sender: send packet[%d]\n", sender_next_seq_num);
        sender_next_seq_num++;
    };

    Sender_StartTimer(timeout);

}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
// |<-  4 byte  ->|<-  2 byte  ->|<-  2 byte  ->|
// | sequence_num |    ack/nack  |  checksum    |
//这里是receiver在回应sender,表示ack或non-ack
void Sender_FromLowerLayer(struct packet *pkt)
{
    ASSERT(pkt != NULL);
    Sender_StartTimer(timeout);

    unsigned int ack_num = 0;
    short ack = -1;
    uint16_t che = 0;

    memcpy(&ack_num, pkt->data, sizeof_sequence_num);
    memcpy(&ack, pkt->data + ack_offset, sizeof_ack);
    memcpy(&che, pkt->data + che_offset, sizeof_checksum);

    if(checksum3(pkt)==che){
        if(ack==1){//ack
            sender_base = ack_num + 1;
            while (sender_next_seq_num < sender_base + window && sender_next_seq_num < sender_window.size()) {
                Sender_ToLowerLayer(&sender_window[sender_next_seq_num]); 
                 printf("Sender success: send packet[%d]\n", sender_next_seq_num);
                sender_next_seq_num++;
            };
        }else if (ack == 0) { // nack message
        unsigned int nack_num = ack_num;
        Sender_ToLowerLayer(&sender_window[nack_num]); //重传
    }
    }

    if (sender_base == sender_next_seq_num) {
        Sender_StopTimer();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{

    Sender_StartTimer(timeout);

    /* resend all packet in the window */
    for (unsigned int i = sender_base; i < sender_next_seq_num; i++) {
        Sender_ToLowerLayer(&sender_window[i]); 
    };

}

