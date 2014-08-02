#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp> 
#include <boost/thread.hpp>
#include <boost/timer.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <initializer_list>

#include <cstdlib>
#include <cstdint>
#include <csignal>
#include <ctime>

#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>

#include "interface.hpp"
#include "usrpd_control.hpp"

#define UDP_PORTNO 5135
#define TCP_PORTNO 5123

#define THRESHOLD 90

#define NUM_ITERATIONS 20

namespace po = boost::program_options;

std::vector<double> to_visit;

boost::mutex spectogram_lock;
boost::mutex mut;

std::vector<uint8_t> spectogram;
std::vector< std::vector<uint8_t> > rx_data;
std::vector<bool> isDone;

boost::condition_variable cProducer;
boost::condition_variable cConsumer;


/**
 * Function to convert from arbitrary numerical type to std::string
 */
template<class T> std::string toString(const T& t)
{
        std::ostringstream stream;
        stream << t;
        return stream.str();
}


/**
 * Function that blocks for a given time in microseconds (real time). 
 * Should be accurate to nanosecond on Unix systems
 */
void mwait(long msecs)
{
    struct timespec start, end, ts_diff;

    clock_gettime(CLOCK_REALTIME, &start);

    do {
        clock_gettime(CLOCK_REALTIME, &end);

        // Corrects for overflow ... 
        if ((end.tv_nsec-start.tv_nsec)<0) {
            ts_diff.tv_sec = end.tv_sec-start.tv_sec-1;
            ts_diff.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        } else {
            ts_diff.tv_sec = end.tv_sec-start.tv_sec;
            ts_diff.tv_nsec = end.tv_nsec-start.tv_nsec;
        }

    } while(ts_diff.tv_nsec <= (msecs * 1000000));
}
/*
void transmit(USRPD_tx tx){
}
*/
void manage_data(int num_rx, unsigned int nbins){

    std::cout << "Data logger thread created\n";

    const std::vector<bool> done(num_rx, true); 
    std::ofstream spectogram_file ("spectogram.dat");
    std::vector<std::ofstream *> rx_files;
    std::string file_name;
    for(int i = 0; i < num_rx; i++){
        file_name = "rx" + toString<int>(i) + ".dat";
        std::ofstream *f = new std::ofstream(file_name.c_str());
        rx_files.push_back(f);
    }

    int counter = 0;
    int num_f = to_visit.size();

    std::cout << "Data logger internals reached\n";

    while(counter < num_f * NUM_ITERATIONS)
    {

        std::cout <<  "Consumer locking mutex before waiting\n";
        boost::unique_lock<boost::mutex> lock(mut);

        while( isDone != done ){
            std::cout << "Consumer waiting for producer to finish\n";
            cConsumer.wait(lock);
        }
        std::cout << "Consumer signaled. Dumping buffers to files\n";

        for(int i = 0; i < num_f * nbins; i++){
            spectogram_file << (unsigned short) spectogram[i] << " ";
        }
        spectogram_file << std::endl;

        for(int i = 0; i < num_rx; i++){
            for (int j = 0; j < num_f * nbins; j++){
                *rx_files[i] << (unsigned short) rx_data[i][j] << " ";
            }
            *rx_files[i] << std::endl;
        }

        isDone = std::vector<bool>(num_rx, false);

        std::cout << "Consumer finished. Notifying producers\n";
        cProducer.notify_all();
        counter++;
    }

    for(int i = 0; i < num_rx; i++){
        delete(rx_files[i]);
    }

}


void sense(USRPD_rx rx, unsigned int avg, int rx_num){
    int num_f = to_visit.size();
    unsigned int nbins = rx.get_status().nbins;
    int f_curr = rand() % num_f;
    rx.switch_freq(to_visit[f_curr]);
    std::vector<unsigned int> avg_data(nbins, 0);
    std::vector<uint8_t> usrpd_data(nbins+1, '\0');

    int counter = 0;

    while(counter < num_f*NUM_ITERATIONS){
        struct rx_stat stat = rx.get_status();
        if(counter % num_f == 0)
            std::cout << stat.name << " rx at iteration: " << counter / num_f << std::endl;

        memset(avg_data.data(), '\0', (nbins)*sizeof(unsigned int));

        for(int i = 0; i < avg; i++){
            memset(usrpd_data.data(), '\0', (nbins+1)*sizeof(uint8_t));
            rx.recv_data(usrpd_data.data(), (nbins+1)*sizeof(uint8_t));
            
            // Cumulative sum
            for(int j = 0; j < nbins; j++){
                avg_data[j] += (unsigned int) usrpd_data[j+1];
            }
        }


        std::cout << stat.name << " producer locking mut\n";
        boost::unique_lock<boost::mutex> lock(mut);
        //boost::this_thread::sleep(boost::posix_time::millisec(500));

        rx_data[rx_num] = std::vector<uint8_t> (nbins+1, 0);
        rx_data[rx_num][0] = f_curr;

        std::cout << "1\n";

        // divide avg_data by number of buffers and compare with threshold
        for( int i = 0; i < avg_data.size(); i++ ){
            avg_data[i] = avg_data[i] / avg + 0.5;
            if (avg_data[i] > THRESHOLD){
                rx_data[rx_num][i+1] = 1;
            } 
        }

        std::cout <<"2\n";

        for( int i = 0; i < avg_data.size(); i++ ){
            boost::unique_lock<boost::mutex> lock(spectogram_lock);
            spectogram[f_curr * 64 + i] = (uint8_t) avg_data[i];
        }
        std::cout<<"3\n";

        isDone[rx_num] = true;

        std::cout << stat.name << " producer done. Notifying consumer\n";
        cConsumer.notify_one();

        f_curr = (f_curr + 1 ) % num_f;
        rx.switch_freq(to_visit[f_curr % num_f]);

        counter++;

        while(isDone[rx_num] == true){
            std::cout << stat.name << " producer waiting for consumer to finish\n";
            cProducer.wait(lock);
        }
        std::cout << stat.name << " producer signaled. \n";

    }

}



int main(int argc, char *argv[]) {
    unsigned int nbins, window, avg;
    double rate, gain, bw, lower_lim, upper_lim;
    std::string input_file, rx_mode;
    std::vector<boost::thread *> rx_threads;
    std::vector<boost::thread *> tx_threads;

    // Nanosecond seed for rand; allows nanosecond calls to rand() as opposed to second calls
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand((time_t)ts.tv_nsec);


    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("nbins", po::value<unsigned int>(&nbins)->default_value(1024), "number of FFT bins")
        ("rate", po::value<double>(&rate)->default_value(1e6), "rate of incoming samples")
        ("gain", po::value<double>(&gain), "gain for the RF chain")
        ("bw", po::value<double>(&bw), "daughterboard IF filter bandwidth in Hz")
        ("lower-lim", po::value<double>(&lower_lim)->default_value(0), "Lower limit to bandwidth of interest")
        ("upper-lim", po::value<double>(&upper_lim)->default_value(0), "Upper limit to bandwidth of interest")
        ("window", po::value<unsigned int>(&window)->default_value(1024), "Length of window used in moving average")
        ("input-file", po::value<std::string>(&input_file)->default_value("nodes.txt"), "File containing nodes to use as receivers")
        ("mode", po::value<std::string>(&rx_mode)->default_value("fftpowudp"), "Receiver module to start; Either fftpowudp or fftavgudp")
        ("avg", po::value<unsigned int>(&avg)->default_value(20), "Number of FFT buffers to average across before thresholding");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")){
        std::cout << boost::format("UHD RX spectrum sense %s") % desc << std::endl;
        return ~0;
    }


    double curr = lower_lim + bw / 2;
    //std::cout << "frequencies to visit \n";
    while( ( curr + rate / 2 ) < upper_lim ){
        to_visit.push_back( curr );
        //std::cout << curr << ", ";
        curr += rate;
    }
    //std::cout << std::endl << std::endl;

    spectogram = std::vector<uint8_t>(64*to_visit.size(), '\0');

    std::ifstream infile; 
    infile.open(&input_file.front());
    std::string input_node;
    uint16_t udp_portno = UDP_PORTNO;
    int rx_num = 0;

    while(!infile.eof()){
        std::getline(infile,input_node);
        if(input_node.empty())
            break;
        size_t tx_at = input_node.find("tx");
        if(tx_at != std::string::npos){
            std::string script = std::string("./restart_usrpd.sh ") + input_node.substr(0, tx_at - 1); 
            system(script.c_str());
            double tx_freq = atof((input_node.substr(tx_at + 3)).c_str());
            //boost::thread thread_(transmit, USRPD_tx(input_node.substr(tx_at - 1), "CONST", tx_freq, rate, 30, bw, 0.7, 0));
            //tx_threads.push_back(thread_);
            std::cout << "Creating transmitter on " << input_node.substr(0, tx_at - 1) << " at " << tx_freq << " Hz\n";
            USRPD_tx tx(input_node.substr(0, tx_at - 1), "CONST", tx_freq, rate, 30, bw, 0.7, 0);
        } 
        else{
            std::string script = std::string("./restart_usrpd.sh ") + input_node; 
            system(script.c_str());
            std::cout << "Creating receiver on " << input_node << std::endl;
            USRPD_rx rx(input_node, rx_mode, lower_lim, rate, gain, bw, nbins, window, TCP_PORTNO, udp_portno);
            boost::thread *thread_ = new boost::thread(sense, rx, avg, rx_num);
            rx_threads.push_back(thread_);
            rx_num++;
            udp_portno++;
        }
    }

    rx_data = std::vector< std::vector<uint8_t> >(rx_num, std::vector<uint8_t>(nbins, '\0'));
    isDone = std::vector<bool>(rx_num, false);

    boost::thread consumer(manage_data, rx_num, nbins);

    consumer.join();
    for(int i = 0; i < rx_threads.size(); i++){
        rx_threads[i]->join();
        delete(rx_threads[i]);
    }



    return 1;
}
