#include "vasp_tool.h"


////////////////////////// Utility Functions //////////////////////////

std::string ExtractKmeshValue(const std::string &filename)
{
    std::ifstream infile(filename);
    std::string line;
    std::string kmeshValue;

    while (std::getline(infile, line))
    {
        if (line.find("is Generally Precise Enough!") != std::string::npos)
        {
            std::istringstream iss(line);
            // 忽略前面的部分，提取 Kmesh-Resolved Value
            std::string token;
            while (iss >> token)
            {
                if (token.find('-') != std::string::npos)
                {
                    kmeshValue = token.substr(0, token.find('-'));
                    break;
                }
            }
            break;
        }
    }

    return kmeshValue;
}

std::string ExtractKSpacingValue(const std::string &filename)
{
    std::ifstream infile(filename);
    std::string line;
    std::string kspacingValue;

    while (std::getline(infile, line))
    {
        if (line.find("is Generally Precise Enough!") != std::string::npos)
        {
            std::istringstream iss(line);
            std::string token;
            while (iss >> token)
            {
                if (token.find('.') != std::string::npos)
                {
                    kspacingValue = token;
                    break;
                }
            }
            break;
        }
    }

    return kspacingValue;
}

void ModifyINCAR(fs::path filepath, const std::map<std::string, std::string> &options)
{
    std::ifstream infile(filepath.string());
    std::stringstream buffer;
    std::string line;

    // Flag to track if a key has been modified
    std::map<std::string, bool> modifiedKeys;
    for (const auto &option : options)
    {
        modifiedKeys[option.first] = false;
    }

    // Read the file line by line
    while (std::getline(infile, line))
    {
        std::string key, value;
        std::size_t pos = line.find('=');

        if (pos != std::string::npos)
        {
            key = line.substr(0, pos);
            value = line.substr(pos + 1);

            // Trim whitespace around key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // If the key exists in options, modify it
            if (options.find(key) != options.end())
            {
                value = options.at(key);
                modifiedKeys[key] = true; // Mark as modified
            }

            buffer << key << " = " << value << std::endl;
        }
        else
        {
            buffer << line << std::endl; // Keep the line unchanged if it does not contain '='
        }
    }

    infile.close();

    // Append new key-value pairs that were not found in the file
    for (const auto &option : options)
    {
        if (!modifiedKeys[option.first])
        {
            buffer << option.first << " = " << option.second << std::endl;
        }
    }

    // Write the modified content back to the file
    std::ofstream outfile(filepath.string());
    outfile << buffer.str();
    outfile.close();
}

// Function to read band.dat file and extract k-points and energies
void ReadBandDat(const std::string &file_path, std::vector<std::vector<double>> &energies)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line[0] != '#')
        { // Skip empty lines and comments
            std::istringstream iss(line);
            double k_point;
            iss >> k_point;

            std::vector<double> energy_levels;
            double energy;
            while (iss >> energy)
            {
                energy_levels.push_back(energy);
            }
            energies.push_back(energy_levels);
        }
    }
    file.close();
}

// Function to calculate the band gap, VBM, and CBM
void CalculateBandgap(const std::vector<std::vector<double>> &energies, double &bandgap, double &VBM, double &CBM)
{
    std::vector<double> VBM_list;
    std::vector<double> CBM_list;

    for (const auto &energy : energies)
    {
        std::vector<double> VBM_candidates;
        std::vector<double> CBM_candidates;

        for (double band : energy)
        {
            if (band < 0)
            {
                VBM_candidates.push_back(band);
            }
            else if (band > 0)
            {
                CBM_candidates.push_back(band);
            }
        }

        if (!VBM_candidates.empty())
        {
            VBM_list.push_back(*std::max_element(VBM_candidates.begin(), VBM_candidates.end()));
        }
        if (!CBM_candidates.empty())
        {
            CBM_list.push_back(*std::min_element(CBM_candidates.begin(), CBM_candidates.end()));
        }
    }

    if (VBM_list.empty())
    {
        throw std::runtime_error("VBM not found: no energy values below 0 eV.");
    }
    if (CBM_list.empty())
    {
        throw std::runtime_error("CBM not found: no energy values above 0 eV.");
    }

    VBM = *std::max_element(VBM_list.begin(), VBM_list.end());
    CBM = *std::min_element(CBM_list.begin(), CBM_list.end());

    bandgap = CBM - VBM;
}


std::vector<ConductivityData> readElectronicConductivity(const std::string &file_path)
{
    std::vector<ConductivityData> conductivities;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue; // Skip comments and empty lines
        std::istringstream iss(line);
        ConductivityData data;
        if (iss >> data.energy >> std::skipws >> data.average_conductivity)
        {
            conductivities.push_back(data);
            // std::cout << "Energy: " << std::fixed << std::setprecision(4)
            //           << data.energy << " eV, Average Conductivity: "
            //           << std::scientific << std::setprecision(4)
            //           << data.average_conductivity << " 1/(Omega*m/s)" << std::endl;
        }
    }
    return conductivities;
}

std::vector<CarrierConcentrationData> readCarrierConcentration(const std::string &file_path)
{
    std::vector<CarrierConcentrationData> concentrations;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue; // Skip comments and empty lines
        std::istringstream iss(line);
        CarrierConcentrationData data;
        if (iss >> data.energy >> data.concentration)
        {
            data.concentration *= 1e20; // Convert to 1/cm^3
            concentrations.push_back(data);
            // std::cout << "Energy: " << std::fixed << std::setprecision(5)
            //           << data.energy << " eV, Carrier Concentration: "
            //           << std::fixed << std::setprecision(4)
            //           << data.concentration << " 1/cm^3" << std::endl;
        }
    }
    return concentrations;
}