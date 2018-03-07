all:
build/domainReal.o: sources/domaincode.cpp baselib build/data
	g++ -std=c++11 -c -O0 -I baselib -include baselib/linux_types.h sources/domaincode.cpp -o build/domainReal.o

build/platformReal.o: sources/platform_api.cpp baselib build/data
	g++ `pkg-config --cflags gtk+-2.0` -std=c++11 -c -O0 -I baselib sources/platform_api.cpp -o build/platformReal.o

build/platformReal.so: build/platformReal.o
	g++ -lwiringPi -ldl  build/platformReal.o -shared -o build/platformReal.so `pkg-config --libs gtk+-2.0`

build/domainReal.so: build/domainReal.o
	g++ build/domainReal.o -shared -o build/domainReal.so

