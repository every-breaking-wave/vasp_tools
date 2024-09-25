import numpy as np
from mendeleev import element



# 读取 OUTCAR 文件中体积
def read_volume_from_outcar(outcar_file):
    with open(outcar_file, 'r') as file:
        for line in file:
            if "volume of cell :" in line:
                return float(line.split()[-1])

# 计算密度
def calculate_density(volume, mass_amu):
    mass_g = mass_amu * 1.66054e-24  # amu 转 g
    volume_cm3 = volume * 1e-24  # Å³ 转 cm³
    density = mass_g / volume_cm3  # g/cm³
    return density


if __name__ == "__main__":
    element_list = ["Si", "O"]
    volume = read_volume_from_outcar("OUTCAR")
    mass_amu = 0
    for element_symbol in element_list:
        mass_amu += element(element_symbol).atomic_weight
    density = calculate_density(volume, mass_amu)
    print(f"Density: {density:.2f} g/cm³")
