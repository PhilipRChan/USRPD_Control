CC = g++
CFLAGS = -std=c++0x -g
LINK = -lboost_program_options -lboost_thread -lboost_timer -lrt

.PHONY: all
all: usrpd_control
	@echo "Everything is up to date"

usrpd_control: main.o usrpd_control.o interface.o
	@echo "=== Linking files ==="
	$(CC) $(CFLAGS) $^ -o $@ $(LINK)

main.o: main.cpp 
	$(CC) $(CFLAGS) -c $< -o $@ $(LINK)

usrpd_control.o: usrpd_control.cpp 
	$(CC) $(CFLAGS) -c $< -o $@ 

interface.o: interface.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo "=== Cleaning up ==="
	@rm -rf *.o
	@rm usrpd_control