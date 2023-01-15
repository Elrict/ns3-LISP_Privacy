# ns3-tfe


This code has been developed in the context of a Master thesis exploring how to provide privacy guarantees to clients using the LISP protocol.

This is a fork of E. Marechal's work that can be found at https://github.com/Emeline-1/ns-3_LISP_NAT

This code simulations are located in `src/addressless/`.


## Installation steps

We first must install the ns3 dependencies and make sure Python2 is the default python installation on the system.
 
```
sudo apt install -y python2 gcc g++
```
Then, we can clone the git repo and build the source code.

```
git clone https://github.com/Elrict/ns-3_LISP_NAT.git 
cd ns-3-lisp
./waf configure
./waf build

```

## Running the simulations

To run the simulations developed, the following command can be used
```
./waf --run "Addressless --SimulationType=... --NbClients=... --Protocol=[TCP/UDP]  --ClientInterval=..." -j10

```