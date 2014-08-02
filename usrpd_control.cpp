#include <iostream>
#include <cstdlib>
#include <sys/types.h>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include "interface.hpp"
#include "usrpd_control.hpp"


/**
 * Function to convert from arbitrary numerical type to std::string
 */
template<class T> std::string toString(const T& t)
{
        std::ostringstream stream;
        stream << t;
        return stream.str();
}

/////////////////////////////////////////////////
/// USRPD class 
/////////////////////////////////////////////////


/**
 * Constructor for USRPD class
 */
USRPD::USRPD(std::string hostname, uint16_t tcp_portno /* = 5123 */ ) : name(hostname), tcp(hostname, tcp_portno) {};

/**
 * Sends a command to USRPD through tcp
 */
int USRPD::send_cmd(std::string cmd) {
    return tcp.send(cmd);
}

/**
 * Gets a response from USRPD through tcp
 */
int USRPD::get_response(std::string &message, size_t length){
    return tcp.receive(message,length);
}


/////////////////////////////////////////////////
/// USRPD_rx class 
/////////////////////////////////////////////////


/**
 * Constructor for USRPD_rx class
 */
USRPD_rx::USRPD_rx(
    std::string hostname, 
    std::string mode,
    double freq, 
    double rate, 
    double gain, 
    double bw, 
    unsigned int nbins, 
    unsigned int window,
    uint16_t tcp_portno /* = 5123 */, 
    uint16_t udp_portno /* = 5135 */
    ) : USRPD(hostname, tcp_portno), status(hostname, mode, freq, rate, gain, bw, nbins, window, udp_portno), udp(udp_portno)
{
    set_freq(freq);
    set_gain(gain);
    set_rate(rate);
    set_bw(bw);
    set_nbins(nbins);
    set_window(window);
    set_port(udp_portno);
    add_rx(mode);
}

struct rx_stat USRPD_rx::get_status(){
    return status;
}

int USRPD_rx::set_freq(double freq_){
    std::string cmd = std::string("set uhd rx_freq ") + toString<double>(freq_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_freq command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.freq = freq_;
        return 1;
    }
}

int USRPD_rx::set_port(uint16_t portno_){
    std::string cmd = std::string("set rx measurement_port ") + toString<uint16_t>(portno_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_freq command" << std::endl;
        return 0;
    }
    else{
        //std::cout << response << std::endl;
        status.portno = portno_;
        return 1;
    }
}

int USRPD_rx::set_gain(double gain_){
    std::string cmd = std::string("set uhd rx_gain ") + toString<double>(gain_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;
    
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_gain command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        status.gain = gain_;
        return 1;
    }
}

int USRPD_rx::set_rate(double rate_){
    std::string cmd = std::string("set uhd rx_rate ") + toString<double>(rate_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;
    
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_rate command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        status.rate = rate_;
        return 1;
    }
}

int USRPD_rx::set_bw(double bw_){
    std::string cmd = std::string("set uhd rx_bandwidth ") + toString<double>(bw_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;
    
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_bw command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        status.bw = bw_;
        return 1;
    }
}

int USRPD_rx::set_nbins(unsigned int nbins_){
    std::string cmd = std::string("set rx numbins ") + toString<int>(nbins_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;
    
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_nbins command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        status.nbins = nbins_;
        return 1;
    }
}

int USRPD_rx::set_window(unsigned int window_){
    std::string cmd = std::string("set rx avgwinlen ") + toString<int>(window_) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_window command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.window = window_;
        return 1;
    }
}

int USRPD_rx::delete_rx(){
    std::string cmd = std::string("delete rx ") + status.mode + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending delete_rx command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        return 1;
    }
}

int USRPD_rx::add_rx(std::string mode){
    if(mode != "fftpowudp" && mode != "fftavgudp"){
        std::cerr << "ERROR: Improper receiver mode specified" << std::endl;
        return 0;
    }
    std::string cmd;
    std::string response;
    mode == "fftpowudp" ? cmd =  std::string("add rx fftpowudp\n") : cmd = std::string("add rx fftavgudp\n");
    //std::cout << cmd;

    int n = send_cmd( cmd );
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set_freq command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        return 1;
    }
}

int USRPD_rx::switch_freq(double freq_){
    delete_rx();
    set_freq(freq_);
    add_rx(status.mode);
}

int USRPD_rx::recv_data(void *buffer, size_t length){
    return udp.receive(buffer, length);
}



/////////////////////////////////////////////////
/// USRPD_tx class 
/////////////////////////////////////////////////


USRPD_tx::USRPD_tx(
    std::string hostname, 
    std::string wavetype,
    double freq,
    double rate,
    double gain,
    double bw, 
    double amplitude,
    double wavefreq,
    uint16_t tcp_portno /* = 5123 */
    ) : USRPD(hostname, tcp_portno), status(hostname, wavetype, freq, rate, gain, bw, amplitude, wavefreq)
{
    set_freq(freq);
    set_gain(gain);
    set_rate(rate);
    set_bw(bw);
    set_amplitude(amplitude);
    set_wavefreq(wavefreq);
    set_wavetype(wavetype);
    add_tx();
}

int USRPD_tx::set_freq(double freq){
    std::string cmd = std::string("set uhd tx_freq ") + toString<double>(freq) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx set_freq command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.freq = freq;
        return 1;
    }
}

int USRPD_tx::set_rate(double rate){
    std::string cmd = std::string("set uhd tx_rate ") + toString<double>(rate) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx set_rate command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.rate = rate;
        return 1;
    }
}

int USRPD_tx::set_gain(double gain){
    std::string cmd = std::string("set uhd tx_gain ") + toString<double>(gain) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;
    
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx set_gain command" << std::endl;
        return 0;
    }
    
    else{
        //std::cout << response << std::endl;
        status.gain = gain;
        return 1;
    }
}

int USRPD_tx::set_bw(double bw){
    std::string cmd = std::string("set uhd tx_rate ") + toString<double>(bw) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx set_bw command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.bw = bw;
        return 1;
    }
}

int USRPD_tx::set_amplitude(double amplitude){
    std::string cmd = std::string("set tx amplitude ") + toString<double>(amplitude) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx set_amplitude command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.amplitude = amplitude;
        return 1;
    }
}

int USRPD_tx::set_wavefreq(double wavefreq){
    std::string cmd = std::string("set tx wavefreq ") + toString<double>(wavefreq) + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending tx wavefreq command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.wavefreq = wavefreq;
        return 1;
    }
}

int USRPD_tx::set_wavetype(std::string wavetype){
    if (wavetype != "SINE" && wavetype != "CONST"){
        std::cerr << "ERROR: Unknown tx wavetype" << std::endl;
        return 0;
    }

    std::string cmd = std::string("set tx wavetype ") + wavetype + "\n";
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending set tx wavetype command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        status.wavetype = wavetype;
        return 1;
    }
}

int USRPD_tx::add_tx(){
    std::string cmd = std::string("add tx waveform\n");
    std::string response;
    //std::cout << cmd;

    int n = send_cmd( cmd );
    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending add_tx command" << std::endl;
        return 0;
    }
    else{
        //std::cout << response << std::endl;
        return 1;
    }
}

int USRPD_tx::delete_tx(){
    std::string cmd = std::string("delete tx waveform\n");
    int n = send_cmd( cmd );
    //std::cout << cmd;
    std::string response;

    if(!n || !get_response(response, 64)){
        std::cerr << "ERROR sending delete_tx command" << std::endl;
        return 0;
    }

    else{
        //std::cout << response << std::endl;
        return 1;
    }
}

struct tx_stat USRPD_tx::get_status(){
    return status;
}

