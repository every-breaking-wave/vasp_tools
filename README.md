# VASP Tool

This project is a VASP tool that can calculate conductivity, dielectric constant, thermal expansion coefficient, and more.


## Usage

1. Clone the repository:

    ```
    git clone https://github.com/every-breaking-wave/vasp_tools
    ```

2. Install the dependencies:

    - Boost: Follow the installation instructions provided by the Boost documentation.
    - Python: Install Python from the official Python website or package management tool.
    - C++11: Ensure that your compiler supports C++11 features.

3. Build the project:

    ```
    cd vasp_tools
    mkdir build 
    cmake -S . -B build
    cd build
    make 
    ```
    Or you can simply run 
    ```
    ./script/compile.sh
    ```
4. Run the server:

    ```
    ./main
    ```

5. Use client to send message
   We also provide a simple client for user to try in **client** folder. You can build and run it.