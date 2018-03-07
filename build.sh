#basic platform
#g++ -O0 -ldl -I baselib sources/pi_platform.cpp -o build/build.o

#platform api
lasttime=`stat build/platformReal.so | grep Modify`
make build/platformReal.so
nowtime=`stat build/platformReal.so | grep Modify`
if [ "$lasttime" != "$nowtime" ]
then
	rm build/platform.so
	ln build/platformReal.so build/platform.so
fi

#domaincode
lasttime=`stat build/domainReal.so | grep Modify`
make build/domainReal.so
nowtime=`stat build/domainReal.so | grep Modify`
if [ "$lasttime" != "$nowtime" ]
then
	rm build/domain.so
	ln build/domainReal.so build/domain.so
fi