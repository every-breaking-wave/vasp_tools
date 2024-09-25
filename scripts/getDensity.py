import numpy as np
from mendeleev import element


# 从 POSCAR 或 CONTCAR 文件中提取元素信息和数量
def get_elements_and_counts_from_poscar(file_path='CONTCAR'):
    with open(file_path, 'r') as file:
        lines = file.readlines()
        elements = lines[5].split()
        counts = list(map(int, lines[6].split()))
    return elements, counts

def get_molar_mass_from_poscar(poscar_file):
    
    # 获取元素符号和原子数量
    elements, numbers = get_elements_and_counts_from_poscar(poscar_file)

    total_molar_mass = 0.0
    
    # 计算总摩尔质量
    for element_symbol, number in zip(elements, numbers):
        # 使用 mendeleev 库获取元素的摩尔质量
        try:
            element_data = element(element_symbol)
            molar_mass = element_data.atomic_weight  # 获取摩尔质量 (g/mol)
            total_molar_mass += molar_mass * number
        except:
            print(f"Warning: Element {element_symbol} not found in mendeleev database.")
    
    return total_molar_mass


# 读取 OUTCAR 文件中体积
def read_volume_from_outcar(outcar_file):
    with open(outcar_file, 'r') as file:
        for line in file:
            if "volume of cell :" in line:
                return float(line.split()[-1])


def calculate_density(outcar_file='OUTCAR', poscar_file='CONTCAR'):
    volume = read_volume_from_outcar(outcar_file)  # Å³
    elements, counts = get_elements_and_counts_from_poscar(poscar_file)
    
    total_mass = 0
    for element_symbol, count in zip(elements, counts):
        molar_mass = element(element_symbol).atomic_weight
        total_mass += molar_mass * count  # g/mol
    
    # 阿伏伽德罗常数
    avogadro_number = 6.022e23
    
    # 体积转换：1 Å³ = 1e-24 cm³
    volume_cm3 = volume * 1e-24
    
    # 密度计算，单位 KG/m³
    density = (total_mass / avogadro_number) / volume_cm3 * 1e3

    return density


if __name__ == "__main__":
    print(calculate_density())
    