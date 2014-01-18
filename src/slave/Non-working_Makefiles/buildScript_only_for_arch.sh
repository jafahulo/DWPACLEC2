echo compiling wpa-crack-s.o
g++ -c -std=c++0x wpa-crack-s.cpp 
echo compiling cpu-crack.o
g++ -c -std=c++0x cpu-crack.cpp 
echo compiling gpu-crack-device.o
nvcc -c gpu-crack-device.cu 
echo compiling gpu-crack-device.o
nvcc -c gpu-crack-host.cu 
echo compiling wpa-crack-s 
g++ -L/usr/local/cuda/lib64 -lcudart -lcrypto -lpthread -o wpa-crack-s wpa-crack-s.o gpu-crack-host.o gpu-crack-device.o cpu-crack.o 


#echo removing object files
#rm *.o
#echo starting wpa-crack-s 3030
#chmod wpa-crack-s 755
#./wpa-crack-s 3030
