build/domainReal.o: sources/domaincode.cpp baselib build/data
	g++ -std=c++11 -c -O0 -I baselib -include baselib/linux_types.h sources/domaincode.cpp -o build/domainReal.o

build/domainReal.so: build/domainReal.o
	g++ build/domainReal.o -std=c++11 -lwiringPi -shared -o build/domainReal.so