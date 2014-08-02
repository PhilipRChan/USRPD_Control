#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h>
#include "interface.hpp"


TCP_Socket::TCP_Socket(std::string hostname, uint16_t portno){
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        std::cerr << "ERROR opening TCP socket" << std::endl;
        exit(1);
    }
    server = gethostbyname(hostname.c_str());
    //std::cout << "In interface.cpp: setting up TCP connection to host " << hostname << std::endl;
    if (server == NULL){
        std::cerr << "ERROR: no such TCP host" << std::endl;
        exit(1);
    }

    if (sockfd < 0){
        std::cerr << "ERROR opening TCP socket" << std::endl;
        exit(1);
    }
    //bzero((char *) &serv_addr, sizeof(serv_addr)); // Depreciated function
    memset((void *) &serv_addr, '\0', sizeof(serv_addr)); // That's better...

    serv_addr.sin_family = AF_INET;

    //bcopy((char *)server->h_addr, 
         //(char *)&serv_addr.sin_addr.s_addr,
         //server->h_length);
    memmove((void *)&serv_addr.sin_addr.s_addr, (void *)server->h_addr, server->h_length);

    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        std::cerr << "ERROR connecting" << std::endl;
        exit(1);
    }
}

TCP_Socket::TCP_Socket( const TCP_Socket& other ) : sockfd(other.sockfd){}


int TCP_Socket::send(std::string message){
    int n = write(sockfd,message.c_str(),message.size()+1);
    if (n < 0) {
         std::cerr << "ERROR writing to socket" << std::endl;
         return 0;
    }
    return 1;
}


int TCP_Socket::receive(std::string &message, size_t length){
    char buffer[length];

    //bzero(buffer, length); 
    memset((void *)buffer, '\0', length); 
    ssize_t n;

    n = read(sockfd,buffer,length-1);
    if (n < 0) {
        std::cerr << "ERROR reading from socket" << std::endl;
        return 0;
    }
    

    //std::cout << buffer << std::endl;

    message.assign(buffer);
    return 1;

}



UDP_Socket::UDP_Socket(uint16_t portno)
{
    struct sockaddr_in address;
    int nonBlocking = 1;

    sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(sockfd<=0){
        std::cerr << "ERROR: Failed to create socket!" << std::endl;

    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(portno);

    std::cout << "Binding to UDP port " << portno << std::endl;

    if (bind(sockfd,(const struct sockaddr*) &address,sizeof(struct sockaddr_in))<0)
    {
        std::cerr << "ERROR: Failed to bind socket" << std::endl;

    }

    if (fcntl(sockfd,F_SETFL,O_NONBLOCK,nonBlocking) == -1)
    {
        std::cerr << "ERROR: Failed to set non-blocking socket" << std::endl;
    }
}

UDP_Socket::UDP_Socket( const UDP_Socket& other ) : sockfd(other.sockfd) {}

int UDP_Socket::receive(void *buffer, size_t length)
{


    //bzero(buffer, length);
    memset(buffer, '\0', length);

    int received_bytes=0;
    unsigned int from_address;
    unsigned int from_port;
    struct sockaddr_in from;
    socklen_t fromLength;
    int counter = 0;

    fromLength = sizeof(from);
    while(received_bytes <= 0){
        received_bytes = recvfrom(sockfd, buffer, length, 0, (struct sockaddr*)&from, &fromLength);
        counter++;
    }

    from_address = ntohl(from.sin_addr.s_addr);
    from_port = ntohs(from.sin_port);

    char ip[16] = {0};
    inet_ntop(AF_INET, &from.sin_addr.s_addr, ip, sizeof (ip));

    char hostname[260];
    char service[260];
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    int response = getnameinfo((sockaddr*)&address, 
                            sizeof(address), 
                            hostname, 
                            260, 
                            service, 
                            260, 
                            0);
    if(response == 0) {
        //std::cout << "host= " << hostname << "@" << ip << ", service=" <<service << ", packet size=" << received_bytes << std::endl;
    }

    //struct udp_from info;
    //info.sender = std::string(hostname);
    //info.packet_size = received_bytes;
    return received_bytes;
}
