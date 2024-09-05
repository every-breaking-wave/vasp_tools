import numpy as np

# 读取数据
data = np.loadtxt('thermal_expansion.dat')
T = data[:, 0]  # 温度
dV_Vref = data[:, 1]  # 体积的相对变化量

# 计算温度差和体积变化率的差分
dT = np.diff(T)
dV_dT_approx = np.diff(dV_Vref) / dT

# 使用所有温度点的平均值来估计TEC
T_mean = np.mean(T[:-1])  # 使用温度点的平均值

# 计算平均热膨胀系数
TEC_approx = np.mean(dV_dT_approx) / T_mean

print(f"Estimated Average Thermal Expansion Coefficient: {TEC_approx:.10f} 1/K (approximate)")

# 将TEC_approx写入到文件中
with open('thermal_expansion_result.txt', 'w') as file:
    file.write(f"{TEC_approx:.10f}\n")

print("Results written to 'thermal_expansion_result.txt'")

