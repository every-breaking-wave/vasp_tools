# g++ main.cpp -o main -lboost_system -lboost_filesystem -lpthread && ./main ~/Materials/SiO2/POSCAR


 g++ main.cpp remote_server.cpp vasp.cpp  -o main -lboost_system -lboost_filesystem -lpthread && ./main