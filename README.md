# ns-3 PA-ATP modules (Work in Progress)
The files are custom modules written based on the conference paper PA-ATP: Progress-Aware Transmission Protocol for In-Network Aggregation, DOI: 10.1109/ICNP59255.2023.10355636

## Installation
1.  Change into the `contrib` directory:

    `$ cd contrib`

2.  Within the `contrib` directory, clone this repository:

    `$ git clone https://github.com/dariuslung/pa-atp-ns3.git pa-atp`

    **Note:** Ensure the directory's name is `pa-atp` to match its libname.

## Running Examples

1.  Configure and build ns-3
    
    `$ ./ns3 configure --enable-examples --enable-tests`
    
    `$ ./ns3 build`

2.  Run an example

    `$ ./ns3 run basic_dumbbell`