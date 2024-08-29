#!/bin/bash
# 第2步
list = 0.95 0.955 0.96 0.965 0.97 0.975 0.98 0.985 0.99 0.995 1 1.005 1.01 1.015 1.02 1.025 1.03 1.035 1.04 1.045 1.05

for i in $list
do
    mkdir -p $i
    cp POSCAR INCAR POTCAR KPOINTS ./$i
    cd ./$i
    sed '2c\'$i'' POSCAR > temp && mv temp POSCAR
    cd ..
done


# 第3步
for i in $list
do
    cd ./$i
    mpirun -np 4 vasp_std
    cd ..
done

# 第4步
mkdir -p CP
j = 1
for i in $list
do
    cp mesh.conf ./$i
    cd ./$i
    phonopy --fc  vasprun.xml 
    phonopy -t mesh.conf > thermo.dat
    cp thermal_properties.yaml ../CP/thermal_properties-$j.yaml
    $((j++))
    cd ..
done


#第5步
for i in $list
do
    cd ./$i
    d=$(awk 'NR==3{print $1}' CONTCAR)
    V=$(echo awk -v x=$i -v a=$d -v b=$d -v c=$d 'BEGIN{printf "%.14f\n",x*x*x*a*b*c}') 
    E=$(grep "TOTEN" OUTCAR | tail -1| awk '{printf "%12.6f \n", $5}')
    echo $V $E >> v-e.dat
    cd ..
done


#第6步
phonopy-qha v-e.dat thermal_properties-{1..11}.yaml > thermo.dat