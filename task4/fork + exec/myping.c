// icmp.cpp
// Robert Iakobashvili for Ariel uni, license BSD/MIT/Apache
//
// Sending ICMP Echo Requests using Raw-sockets.
//

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>

// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

// 1. Change SOURCE_IP and dst_ip to the relevant
//     for your computer
// 2. Compile it using MSVC compiler or g++
// 3. Run it from the account with administrative permissions,
//    since opening of a raw-socket requires elevated preveledges.
//
//    On Windows, right click the exe and select "Run as administrator"
//    On Linux, run it as a root or with sudo.
//
// 4. For debugging and development, run MS Visual Studio (MSVS) as admin by
//    right-clicking at the icon of MSVS and selecting from the right-click
//    menu "Run as administrator"
//
//  Note. You can place another IP-source address that does not belong to your
//  computer (IP-spoofing), i.e. just another IP from your subnet, and the ICMP
//  still be sent, but do not expect to see ICMP_ECHO_REPLY in most such cases
//  since anti-spoofing is wide-spread.

#define SOURCE_IP "10.0.2.15"
// i.e the gateway or ping to google.com for their ip-address

int main(int count, char *argv[])
{
    if (count != 2)
    {
        printf("usage: %s <ip> \n", argv[0]);
        exit(0);
    }
    char *dst_ip = argv[1];
    count = 0;
    struct icmp icmphdr; // ICMP-header
    char data[IP_MAXPACKET] = "This is the ping.\n";

    int datalen = strlen(data) + 1;

    // Create raw socket for IP-RAW (make IP-header by yourself)
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    int flag = 1;  // flag for the first print of info
    int index = 0; // index for icmp seq
    //===================
    // ICMP header
    //===================
loop:
    // Message Type (8 bits): ICMP_ECHO_REQUEST
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): some number to trace the response.
    // It will be copied to the response packet and used to map response to the request sent earlier.
    // Thus, it serves as a Transaction-ID when we need to make "ping"
    icmphdr.icmp_id = 18;

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = count++;

    // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
    icmphdr.icmp_cksum = 0;

    // Combine the packet
    char packet[IP_MAXPACKET];

    // Next, ICMP header
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    // After ICMP header, add the ICMP data.
    memcpy(packet + ICMP_HDRLEN, data, datalen);

    // Calculate the ICMP header checksum
    icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;

    // The port is irrelant for Networking and therefore was zeroed.
    // dest_in.sin_addr.s_addr = iphdr.ip_dst.s_addr;
    dest_in.sin_addr.s_addr = inet_addr(dst_ip);
    // inet_pton(AF_INET, dst_ip, &(dest_in.sin_addr.s_addr));

    struct timeval start, end;
    gettimeofday(&start, 0);

    // Send the packet using sendto() for sending datagrams.
    int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
    if (bytes_sent == -1)
    {
        fprintf(stderr, "sendto() failed with error: %d", errno);
        return -1;
    }
    // printf("Successfuly sent one packet : ICMP HEADER : %d bytes, data length : %d , icmp header : %d \n", bytes_sent, datalen, ICMP_HDRLEN);
    if (flag)
    {
        printf("PING %s (%s): %d data bytes\n", dst_ip, dst_ip, bytes_sent);
        flag = 0;
    }

    // Get the ping response
    bzero(packet, IP_MAXPACKET);
    socklen_t len = sizeof(dest_in);
    size_t bytes_received = -1;
    while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
    {
        if (bytes_received > 0)
        {
            // Check the IP header
            struct iphdr *iphdr = (struct iphdr *)packet;
            struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
            // printf("%ld bytes from %s\n", bytes_received, inet_ntoa(dest_in.sin_addr));
            // icmphdr->type

            gettimeofday(&end, 0);

            char reply[IP_MAXPACKET];
            memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);
            // printf("ICMP reply: %s \n", reply);

            char sourceIPAddrReadable[32] = {'\0'};
            inet_ntop(AF_INET, &iphdr->saddr, sourceIPAddrReadable, sizeof(sourceIPAddrReadable));
            char destinationIPAddrReadable[32] = {'\0'};
            inet_ntop(AF_INET, &iphdr->daddr, destinationIPAddrReadable, sizeof(destinationIPAddrReadable));

            float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
            unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
            printf("%ld bytes from %s to %s: icmp_seq=%d time=%f ms\n", bytes_received, sourceIPAddrReadable, destinationIPAddrReadable,
             icmphdr->un.echo.sequence, milliseconds);

            break;
        }
    }

    goto loop;

    // Close the raw socket descriptor.
    close(sock);

    return 0;
}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}
