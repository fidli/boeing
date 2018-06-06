#basic platform
g++ -O0 -ldl -std=c++11 -ggdb -DCRT_PRESENT -I baselib sources/pi_platform.cpp -lwiringPi -lpthread -o build/build.o

#domaincode
lasttime=`stat build/domainReal.so | grep Modify`
make build/domainReal.so
nowtime=`stat build/domainReal.so | grep Modify`
if [ "$lasttime" != "$nowtime" ]
then
	rm build/domain.so
	ln build/domainReal.so build/domain.so
fi