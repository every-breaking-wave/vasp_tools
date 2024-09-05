#!/bin/bash
# 第2步
nums=(0.95 0.955 0.96 0.965 0.97 0.975 0.98 0.985 0.99 0.995 1 1.005 1.01 1.015 1.02 1.025 1.03 1.035 1.04 1.045 1.05)  

# nums=(0.99 0.995 1 1.005 1.01)  

for i in "${nums[@]}"  
do
    mkdir -p $i
    cp POSCAR INCAR POTCAR KPOINTS ./$i
    cd "./${i}"
    sed '2c\'$i'' POSCAR > temp && mv temp POSCAR
    cd ..
done


# 第3步
for i in "${nums[@]}"  
do
    cd "./${i}"
    mpirun -np 4 vasp_std
    cd ..
done

# 第3步
mkdir -p CP
j = 1
for i in "${nums[@]}"  
do
    cp mesh.conf ./$i
    cd "./${i}"
    phonopy --fc  vasprun.xml 
    phonopy -t mesh.conf > thermo.dat
    cp thermal_properties.yaml ../CP/thermal_properties-$j.yaml
    $((j++))
    cd ..
done


#第5步
for i in "${nums[@]}"  
do
    # i 为  1，跳过
    if [ "$i" == "1" ]; then
        continue
    fi
    
    cd "./${i}" || exit  # 尝试进入目录，如果失败则退出脚本  
    
    # 初始化一个float类型的变量，从CONTCAR文件中读取  
    d=$(awk 'NR==3{print $1}' CONTCAR)  
  
    # 使用awk计算V的值，这里假设d, a, b, c都是d的值  
    V=$(awk -v x="$i" -v d="$d" 'BEGIN{printf "%.14f\n", x*x*x*d*d*d}')  
  
    # 从OUTCAR文件中提取TOTEN的值  
    E=$(grep "TOTEN" OUTCAR | tail -1 | awk '{printf "%12.6f\n", $5}')  
  
    cd ..  # 返回上一级目录  
  
    # 将V和E的值追加到v-e.dat文件中  
    echo "$V $E" >> v-e.dat  
done


#第6步
phonopy-qha v-e.dat ./CP/thermal_properties-{1..21}.yaml > thermo.dat

#第7步
# 确保thermal_expansion.dat文件生成
# 运行python3 computeThermalExpansion.py
python3 computeThermalExpansion.py


#第8步 计算比热容
# 使用 PHONOPY 计算比热容 (C_v)
phonopy --cp --t thermal_properties.yaml

# 第9步: 计算热导率
# 对于热导率，可能需要结合第一性原理计算结果与更复杂的模型，如
# Boltzmann Transport Equation (BTE) 或其他理论模型
# 如果使用 ShengBTE, 需要以下步骤:
# 1. 生成 FORCE_CONSTANTS 文件: phonopy --fc vasprun.xml > FORCE_CONSTANTS
# 2. 创建第二个配置文件，如 ShengBTE.conf
# 3. 运行 ShengBTE: shengbte ShengBTE.conf
