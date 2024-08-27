#!/bin/bash

# 删除文件
rm -rf vasp_*

#  编译
g++ vasp.cpp -o main -lboost_system -lboost_filesystem -lpthread 

# 运行
 ./main ~/Materials/SiO2/POSCAR