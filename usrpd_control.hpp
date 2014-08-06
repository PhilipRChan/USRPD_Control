#ifndef _USRPD_CONTROL_H_
#define _USRPD_CONTROL_H_

#include <string>
#include <sys/types.h>
#include "interface.hpp"


struct rx_stat{
    std::string name;
    std::string mode;
    double freq;
    double rate;
    double gain;
    double bw;
    unsigned int nbins;
    unsigned int window;
    uint16_t portno;
    rx_stat(std::string name, std::string mode, double freq, double rate, double gain, double bw, unsigned int nbins, unsigned int window, uint16_t portno
        ) : name(name), mode(mode), freq(freq), rate(rate), gain(gain), bw(bw), nbins(nbins), window(window), portno(portno) {};
};

struct tx_stat{
    std::string name;
    std::string wavetype;
    double freq;
    double rate;
    double gain;
    double bw;
    double amplitude;
    double wavefreq;
    tx_stat(std::string name, std::string wavetype, double freq, double rate, double gain, double bw, double amplitude, double wavefreq
        ) : name(name), wavetype(wavetype), freq(freq), rate(rate), gain(gain), bw(bw), amplitude(amplitude), wavefreq(wavefreq) {};
};


class USRPD{
private:
    TCP_Socket tcp;
    std::string name;
public:
    USRPD(std::string hostname, uint16_t tcp_portno = 5123);
    int send_cmd(std::string cmd);
    int get_response(std::string &buffer, size_t length);
};


class USRPD_rx : public USRPD {
private:
    struct rx_stat status;
    UDP_Socket udp;
    int set_freq(double freq);
    int set_rate(double rate);
    int set_gain(double gain);
    int set_bw(double bw);
    int set_nbins(unsigned int nbins);
    int set_window(unsigned int window);
    int set_port(uint16_t portno);

public:
    USRPD_rx(
        std::string hostname, 
        std::string mode,
        double freq, 
        double rate, 
        double gain, 
        double bw, 
        unsigned int nbins, 
        unsigned int window,
        uint16_t tcp_portno = 5123, 
        uint16_t udp_portno = 5135
    );

    int switch_freq(double freq);
    struct rx_stat get_status();
    int recv_data(void *buffer, size_t length);
    int delete_rx();
    int add_rx(std::string mode);
};

class USRPD_tx : public USRPD {
private:
    struct tx_stat status;
    int set_rate(double rate);
    int set_gain(double gain);
    int set_bw(double bw);
    int set_amplitude(double amplitude);
    int set_wavefreq(double wavefreq);
    int set_wavetype(std::string wavetype);

public:
    USRPD_tx(
        std::string hostname, 
        std::string wavetype,
        double freq,
        double rate,
        double gain,
        double bw,
        double amplitude,
        double wavefreq,
        uint16_t tcp_portno = 5123
    );
    int add_tx();
    int delete_tx();
    int set_freq(double freq);
    //int tx_sleep(double t_sleep);
    struct tx_stat get_status();
};

#endif