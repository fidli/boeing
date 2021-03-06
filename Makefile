build/domainReal.o: sources/pi_domain.cpp sources/messages.h baselib build/data sources/algorithms.h
	g++ -std=c++11 -c -O0 -I baselib -include baselib/linux_types.h -include baselib/common.h -include sources/pi_domain_mem.h -include baselib/util_mem.h -DCRT_PRESENT -include baselib/linux_net.cpp -include baselib/linux_thread.cpp -include string.h -include baselib/linux_filesystem.cpp -DDEBUG sources/pi_domain.cpp -o build/domainReal.o

build/domainReal.so: build/domainReal.o
	g++ build/domainReal.o -std=c++11 -lwiringPi -shared -o build/domainReal.so