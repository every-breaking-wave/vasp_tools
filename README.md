# VASP Auxiliary Tool

This project is a VASP auxiliary tool that can calculate conductivity, dielectric constant, thermal expansion coefficient, and more.


## Usage

1. Clone the repository:

    ```
    git clone https://github.com/every-breaking-wave/vasp_tools
    ```

2. Install the dependencies:

    - Boost: Follow the installation instructions provided by the Boost documentation.
    - Python: Install Python from the official Python website.
    - C++11: Ensure that your compiler supports C++11 features.

3. Build the project:

    ```
    cd vasp_tools
    g++ main.cpp -o main -lboost_system -lboost_filesystem -lpthread
    ```

4. Run the tool:

    ```
    ./main <POSCAR_PATH>
    ```
