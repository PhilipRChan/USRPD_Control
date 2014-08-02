#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <string>
#include <cstdint>
#include <sys/types.h>




/**
 * Class definition for TCP client 
 * Does not contain TCP server implementation as it is not needed to control USRPD
 */
class TCP_Socket{
public:
    TCP_Socket(std::string hostname, uint16_t portno = 5123); // Constructor: creates TCP socket
    TCP_Socket( const TCP_Socket& other );
    int receive(std::string &message, size_t length); // Member function to receive from TCP socket into buffer
    int send(std::string message); // Member function to stream message through TCP socket
private:
    int sockfd; // Socket file descriptor
};


/**
 * Class definition for UDP client 
 * Does not contain UDP server implementation or method to send UDP packets
 */
class UDP_Socket{
public:
    UDP_Socket( uint16_t portno = 5135); // Constructor: creates UDP socket
    UDP_Socket( const UDP_Socket& other );
    int receive(void *message, size_t length); // Member function to read packet from UDP socket into void *buffer
private:
    int sockfd; // 
};

#endif