import numpy as np
import yaml
from mendeleev import element
from getDensity import get_molar_mass_from_poscar
import os


def read_thermal_properties(file_path):
    with open(file_path, 'r') as f:
        data = yaml.safe_load(f)
    temperatures = [entry['temperature'] for entry in data['thermal_properties']]
    heat_capacity = [entry['heat_capacity'] for entry in data['thermal_properties']]
    return temperatures, heat_capacity


# 定义读取多组比热容数据的函数
def read_multiple_thermal_properties(dir_path):
    all_temperatures = []
    all_heat_capacities = []
    
    file_list = os.listdir(dir_path)

    for file in file_list:
        temperatures, heat_capacity = read_thermal_properties(dir_path + "/" +file)
        all_temperatures.append(temperatures)
        all_heat_capacities.append(heat_capacity)
    
    # 假设每个文件的温度范围一致，可以取第一个文件的温度作为基准
    base_temperatures = all_temperatures[0]
    heat_capacities_mean = np.mean(all_heat_capacities, axis=0)
    heat_capacities_std = np.std(all_heat_capacities, axis=0)
    
    return base_temperatures, heat_capacities_mean, heat_capacities_std

# 提供 thermal_properties 文件路径列表
dir_path = "CP"
temperatures, heat_capacity_mean, heat_capacity_std = read_multiple_thermal_properties(dir_path)


# 获取常温下的比热容 （300K左右，由于数据点不够，这里取最接近的数据点）
T_300 = temperatures[np.argmin(np.abs(np.array(temperatures) - 300))]

#  目前单位是 J/K/mol， 将单位转化为 J/kg*K

mol_mass = get_molar_mass_from_poscar("POSCAR")

cp_300 = heat_capacity_mean[np.argmin(np.abs(np.array(temperatures) - 300))] / mol_mass * 1000


with open('specified_heat_result.txt', 'w') as file:
    file.write(f"{cp_300:.3f}")

print("Results written to 'specified_heat_result.txt'")

