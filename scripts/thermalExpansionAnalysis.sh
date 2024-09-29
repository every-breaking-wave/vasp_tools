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
    mpirun -np 16 vasp_std > ../log-$i
    cd ..
done

# 将上述的日志文件合并到一个文件中
cat log-* > vasp_thermal_expansion.log


# 第3步
mkdir -p CP
j=1
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
# 清空v-e.dat文件
echo "" > v-e.dat

for i in "${nums[@]}"  
do    
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
# phonopy-qha v-e.dat ./CP/thermal_properties-{1..21}.yaml > thermo.dat
# 获取nums数组的长度
# length=${#nums[@]}
# echo "length is $length"
phonopy-qha v-e.dat ./CP/thermal_properties-{1..21}.yaml > thermo.dat

#第7步 计算热膨胀系数
# 确保thermal_expansion.dat文件生成
# 运行python3 computeThermalExpansion.py
python3 computeThermalExpansion.py


#第8步 计算比热容
# 使用 PHONOPY 计算比热容 (C_v),并将结果保存到文件中
python3 computeSpecifiedHeat.py

# 第9步: 计算热导率
# NOTE: 以下计算过程是可行的，但是计算时间过长，暂不使用
# conda activate phono3py

# phono3py -d --dim="1 1 1" -c POSCAR

# # 获取产生了多少个POSCAR文件
# num_poscar=$(ls -l | grep POSCAR | wc -l) - 1
# echo "产生了 $num_poscar 个POSCAR文件"


# for i in {00001..00005}; do
#     mkdir $i
#     cp POSCAR-$i ./$i/POSCAR
#     cp INCAR KPOINTS POTCAR ./$i
#     cd ./$i
#     mpirun -np 12 vasp_std
#     cd ../
# done

# # 提取三阶力常数
# phono3py --cf3 {00001..00005}/vasprun.xml

# # 提取二阶力常数
# phono3py --dim="1 1 1" --sym-fc -c POSCAR

# # 计算热导率。
# phono3py --fc3 --fc2 --dim="1 1 1" --mesh="19 19 19" -c POSCAR --br | tee thermal_conductivity.txt

# conda deactivate