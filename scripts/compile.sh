# g++ main.cpp -o main -lboost_system -lboost_filesystem -lpthread && ./main ~/Materials/SiO2/POSCAR


#  g++ main.cpp ./src/remote_server.cpp ./src/vasp.cpp ./src/vasptool.cpp  -o main -lboost_system -lboost_filesystem -lpthread && ./main


 rm -rf build && mkdir build &&  cmake -S . -B build && cd build && make && cd ..
