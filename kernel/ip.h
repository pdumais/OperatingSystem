#pragma once

#define IP_SEND_ERROR_NO_MAC -1
#define IP_SEND_ERROR_HW -2

int ip_send(unsigned long sourceInterface, unsigned int destIP, 
    char* buffers, unsigned short size, unsigned char protocol);
void ip_process(struct Layer2Payload* payload);

