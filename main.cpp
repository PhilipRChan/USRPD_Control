#include <boost/program_options.hpp>
#include <boost/format.hpp> 
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp> 
#include <boost/thread.hpp>
#include <boost/timer.hpp>

#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <cstdlib>
#include <cstdint>
#include <ctime>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "usrpd_control.hpp"

#define UDP_PORTNO 5135
#define TCP_PORTNO 5123

#define THRESHOLD 50


namespace po = boost::program_options;

std::vector<double> to_visit; // Vector of receiver center-frequencies

boost::mutex spectogram_lock; // Mutex for spectogram vector

// Data structures for storing received data
std::vector<uint8_t> spectogram;
std::vector< std::vector<uint8_t> > rx_data;

// For synchronization between receivers and data-logger
boost::mutex mut;
std::vector<bool> isDone;

// Condition variables for synchronization between receivers and data-logger
boost::condition_variable cProducer;
boost::condition_variable cConsumer;

std::atomic<bool> finished(false); // Atomic bool to signal transmitters to exit


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
 * Function for transmitter threads
 * Randomly turns transmitter on/off in intervals of 40ms to 2s
 * 
 * NOT synchronized with RX or data logger threads
 */

void transmit(USRPD_tx tx, double lower_lim, double upper_lim){
    long t_sleep;
    
    while(!finished){
        // Stay on for random time
        t_sleep = (long) (rand() % 1961 + 40);
        boost::this_thread::sleep(boost::posix_time::milliseconds(t_sleep));

        // turn off for random time
        tx.delete_tx();
        t_sleep = (long) (rand() % 1961 + 40);
        boost::this_thread::sleep(boost::posix_time::milliseconds(t_sleep));
        tx.add_tx();
    }

    tx.delete_tx();
}



/**
 * Data logger thread
 * Formats vectors from receivers, time-stamps, and prints to .dat files
 */

void manage_data(int num_rx, unsigned int nbins, unsigned int nIter){
    const std::vector<bool> done(num_rx, true); // Bool vector of all trues

    std::ofstream spectogram_file ("spectogram.dat"); // File for complete spectogram

    std::vector<std::ofstream *> rx_files; // Files for individual receivers
    std::string file_name;
    for(int i = 0; i < num_rx; i++){
        file_name = "rx" + toString<int>(i) + ".dat";
        std::ofstream *f = new std::ofstream(file_name.c_str());
        rx_files.push_back(f);
    }

    struct timespec start, end, ts_diff;
    unsigned long t_avg;
    int counter = 0;
    int nFreq = to_visit.size();

    // start timer
    clock_gettime(CLOCK_REALTIME, &start);

    while(counter < nFreq * nIter)
    {

        // Wait for Receivers to produce data and signal as done
        boost::unique_lock<boost::mutex> lock(mut);
        while( isDone != done ){
            cConsumer.wait(lock);
        }

        // Conglomerate rx data into single spectogram and send average log energy data to file
        for(int i = 0; i < nFreq * nbins; i++){
            spectogram_file << (unsigned short) spectogram[i] << " ";
        }
        spectogram_file << std::endl;


        // Store individual rx data: 1 for Rx at given frequency, 0 for Rx not at given freqency
        for(int i = 0; i < num_rx; i++){
            int counter = 0;
            uint8_t fcurr = rx_data[i][0];

            for(int j = 0; j < (int) fcurr * nbins; j++){
                *rx_files[i] << 0 << " ";
                counter++;
            }

            for (int j = 0; j < nbins; j++){
                //*rx_files[i] << (unsigned short) rx_data[i][j + 1] << " ";
                *rx_files[i] << 1 << " ";
                counter++;
            }
            while(counter < nFreq * nbins){
                *rx_files[i] << 0 << " ";
                counter++;
            }
            *rx_files[i] << std::endl;
        }

        // Reset rx done flags
        isDone = std::vector<bool>(num_rx, false);

        // Notify receivers to continue to next freqency
        cProducer.notify_all();
        counter++;

        clock_gettime(CLOCK_REALTIME, &end);

        if((end.tv_nsec-start.tv_nsec)<0){
            ts_diff.tv_sec = end.tv_sec-start.tv_sec-1;
            ts_diff.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
        }
        else{
            ts_diff.tv_sec = end.tv_sec-start.tv_sec;
            ts_diff.tv_nsec = end.tv_nsec-start.tv_nsec;
        }
        t_avg += ts_diff.tv_nsec / 1e6;
    }

    // Free pointers to heap
    for(int i = 0; i < num_rx; i++){
        delete(rx_files[i]);
    }

    t_avg /= counter;
    std::cout << "Average time resolution: " << t_avg << " ms" << std::endl;

    std::ofstream t_file ("time.dat"); // File storing time resolution
    t_file << t_avg << std::endl;


    // Signal transmitters to exit
    finished = true;
}

/**
 * Function for receiver threads
 * 
 * Synchronized with data-logger thread
 */
 
void sense(USRPD_rx rx, unsigned int avg, int rx_num, unsigned int nIter){
    int nFreq = to_visit.size();
    unsigned int nbins = rx.get_status().nbins;
    double rate = rx.get_status().rate;
    int f_curr = rand() % nFreq;
    rx.switch_freq(to_visit[f_curr]);
    std::vector<unsigned int> avg_data(nbins, 0);
    std::vector<uint8_t> usrpd_data(nbins+1, '\0');

    int counter = 0;

    // Wait to make sure data-logger thread started
    boost::this_thread::sleep(boost::posix_time::milliseconds(50));

    while(counter < nFreq*nIter){
        struct rx_stat stat = rx.get_status();
        if(counter % nFreq == 0)

        memset(avg_data.data(), '\0', (nbins)*sizeof(unsigned int));

        for(int i = 0; i < avg; i++){
            memset(usrpd_data.data(), '\0', (nbins+1)*sizeof(uint8_t));
            rx.recv_data(usrpd_data.data(), (nbins+1)*sizeof(uint8_t));
            
            // Cumulative sum
            for(int j = 0; j < nbins; j++){
                avg_data[j] += (unsigned int) usrpd_data[j+1];
            }
        }


        boost::unique_lock<boost::mutex> lock(mut);

        rx_data[rx_num] = std::vector<uint8_t> (nbins+1, 0);
        rx_data[rx_num][0] = f_curr;

        // divide avg_data by number of buffers and compare with threshold
        for( int i = 0; i < avg_data.size(); i++ ){
            avg_data[i] = avg_data[i] / avg + 0.5;

            if (avg_data[i] > THRESHOLD){
                rx_data[rx_num][i+1] = 1;
            } 
        }

        // Write averaged energy data to spectogram vector
        for( int i = 0; i < avg_data.size(); i++ ){
            boost::unique_lock<boost::mutex> lock(spectogram_lock);
            spectogram[f_curr * nbins + i] = (uint8_t) avg_data[i];
        }

        // Set done flag and tell data-logger to start
        isDone[rx_num] = true;
        cConsumer.notify_one();

        // Switch to next frequency
        f_curr = (f_curr + 1 ) % nFreq;
        rx.switch_freq(to_visit[f_curr % nFreq]);

        counter++;

        // Wait for data-logger to finish
        while(isDone[rx_num] == true){
            cProducer.wait(lock);
        }

    }
    rx.delete_rx();
}


int main(int argc, char *argv[]) {
    unsigned int nbins, window, avg, nIter;
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
        ("nbins", po::value<unsigned int>(&nbins)->default_value(64), "number of FFT bins")
        ("rate", po::value<double>(&rate)->default_value(2e6), "rate of incoming samples")
        ("gain", po::value<double>(&gain)->default_value(30), "gain for the RF chain")
        ("bw", po::value<double>(&bw)->default_value(2e6), "daughterboard IF filter bandwidth in Hz")
        ("lower-lim", po::value<double>(&lower_lim)->default_value(1.48e9), "Lower limit to bandwidth of interest")
        ("upper-lim", po::value<double>(&upper_lim)->default_value(1.52e9), "Upper limit to bandwidth of interest")
        ("window", po::value<unsigned int>(&window)->default_value(64), "Length of window used in moving average")
        ("input-file", po::value<std::string>(&input_file)->default_value("nodes.txt"), "File containing nodes to use as receivers")
        ("mode", po::value<std::string>(&rx_mode)->default_value("fftpowudp"), "Receiver module to start; Either fftpowudp or fftavgudp")
        ("avg", po::value<unsigned int>(&avg)->default_value(10), "Number of FFT buffers to average across before thresholding")
        ("niterations", po::value<unsigned int>(&nIter)->default_value(50), "Number of times to sweep through frequencies before exiting");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")){
        std::cout << boost::format("UHD RX spectrum sense %s") % desc << std::endl;
        return ~0;
    }


    double curr = lower_lim + bw / 2;
    while( ( curr + rate / 2 ) < upper_lim ){
        to_visit.push_back( curr );
        curr += rate;
    }

    spectogram = std::vector<uint8_t>(nbins*to_visit.size(), '\0');

    std::ifstream infile; 
    infile.open(&input_file.front());
    std::string input_node;
    uint16_t udp_portno = UDP_PORTNO;
    int rx_num = 0;


    // Parse data from nodes.txt
    while(!infile.eof()){
        // read in line
        std::getline(infile,input_node);
        if(input_node.empty())
            break;

        // Check if line for tx
        size_t tx_at = input_node.find("tx");
        // If so, create tx thread
        if(tx_at != std::string::npos){
            std::string script = std::string("./restart_usrpd.sh ") + input_node.substr(0, tx_at - 1); 
            //system(script.c_str());
            double tx_freq = atof((input_node.substr(tx_at + 3)).c_str());
            USRPD_tx tx(input_node.substr(0, tx_at - 1), "CONST", tx_freq, rate, 30, bw, 0.7, 0);
            boost::thread *thread_ = new boost::thread(transmit, tx, lower_lim, upper_lim);
            tx_threads.push_back(thread_);
            std::cout << "Creating transmitter on " << input_node.substr(0, tx_at - 1) << " at " << tx_freq << " Hz\n";
        } 
        // Otherwise create rx thread with incrementing udp port number
        else{
            std::string script = std::string("./restart_usrpd.sh ") + input_node; 
            //system(script.c_str());
            std::cout << "Creating receiver on " << input_node << std::endl;
            USRPD_rx rx(input_node, rx_mode, lower_lim, rate, gain, bw, nbins, window, TCP_PORTNO, udp_portno);
            boost::thread *thread_ = new boost::thread(sense, rx, avg, rx_num, nIter);
            rx_threads.push_back(thread_);
            rx_num++;
            udp_portno++;
        }
    }

    // Initialize data structs
    rx_data = std::vector< std::vector<uint8_t> >(rx_num, std::vector<uint8_t>(nbins + 1, '\0'));
    isDone = std::vector<bool>(rx_num, false);

    // Create data-logger thread
    boost::thread consumer(manage_data, rx_num, nbins, nIter);
    std::cout << "Creating consumer thread" << std::endl;

    // Join threads
    consumer.join();
    std::cout << "Consumer joined" << std::endl;
    for(int i = 0; i < rx_threads.size(); i++){
        rx_threads[i]->join();
        std::cout << "Rx joined" << std::endl;
        delete(rx_threads[i]);
    }
    for(int i = 0; i < tx_threads.size(); i++){
        tx_threads[i]->join();
        std::cout << "Tx joined" << std::endl;
        delete(tx_threads[i]);
    }
    return 1;
}
