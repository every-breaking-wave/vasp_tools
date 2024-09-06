import sys

def read_band_dat(file_path):
    k_points = []
    energies = []

    with open(file_path, 'r') as file:
        for line in file:
            if line.strip() and not line.startswith("#"):  # Skip empty lines and comments
                try:
                    data = list(map(float, line.split()))
                    k_points.append(data[0])
                    energies.append(data[1:])  # Collect all energy levels for this k-point
                except ValueError:
                    # Handle lines that cannot be converted to floats
                    print(f"Skipping line: {line.strip()}")
                    continue

    return k_points, energies

def calculate_bandgap(energies):
    VBM_list = []
    CBM_list = []

    for energy in energies:
        VBM_candidates = [band for band in energy if band < 0]
        CBM_candidates = [band for band in energy if band > 0]

        if VBM_candidates:
            VBM_list.append(max(VBM_candidates))
        if CBM_candidates:
            CBM_list.append(min(CBM_candidates))

    if not VBM_list:
        raise ValueError("VBM not found: no energy values below 0 eV.")
    if not CBM_list:
        raise ValueError("CBM not found: no energy values above 0 eV.")

    VBM = max(VBM_list)
    CBM = min(CBM_list)

    return CBM - VBM, VBM, CBM


if __name__ == '__main__':
    # 第一个参数为文件路径
    filepath = sys.argv[1]
    k_points, energies = read_band_dat(filepath)
    bandgap, VBM, CBM = calculate_bandgap(energies)

    print(f"Valence Band Maximum (VBM): {VBM} eV")
    print(f"Conduction Band Minimum (CBM): {CBM} eV")
    print(f"Bandgap: {bandgap} eV")

