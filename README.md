modbus demo
===========

## modbus server

This is a simple MODBUS Slave.

The following tables are supported.

- 8 of Discretes Input.
- 8 of Coils.
- 8 of Input Registers
- 8 of Holding Registers

A modbus client can read value from any of registers,
and can read value from any of Discretes Input or Input Registers.

The following registers are special ones.
The modbus server set a waving value into the address 0 of Input Registers
periodically.
A modbus client can put a base value of the waving value into
the address 0 of Holding Registers.

So, the waving value is calculated by the following expression,
where as t is a internal timer.

    ~~~~
    waving value = base value + sign(t*3.14/180) + sign((t*2)*3.14/180)
    ~~~~

### usage

    ~~~~
    % modbus_server -h
    Usage: modbus_server [-S host] [-P port] [-dh]
    ~~~~

By default, the server listens all interfaces with TCP port number 50200.

## modbus client

This is a simple MODBUS Master.

    ~~~~
    % modbus_client -h
    Usage: modbus_client [-P port] [-j job] [-dh] (host)
        job: 0: let mode low
             1: let mode high
             2: read mode
             3: read value (default)
    ~~~~

By default, the client send a message to TCP port number 50200.
With the job#0, it will try to set 27 to the base value of the modbus server.
With the job#1, it will try to set 18 to that.

## requirement

libmodbus is required.

This server/client program use a modified version of libmodbus.
[https://github.com/tanupoo/libmodbus.git]

You have to take and install it.
The following instruction is an example for that.

    ~~~~
    git clone https://github.com/tanupoo/libmodbus.git
    cd libmodbus
    git checkout -b listen_maddr
    sh autogen.sh
    ./configure
    make
    make install
    ~~~~

actually, building libmodbus requires the following packages.
so you need to install them before you build libmodbus.

    autoconf
    automake
    libtool

if you want to install libmodbus into uncommon place,
also you have to consider the path for building the software of modbus.

### base value

- register and address: Holding Register: 0
- modbus code to write: Write Single Register (06)
- modbus code to read: Read Holding Registers (03)

### waving value

- register and address: Input Register: 0
- modbus code to read: Read Input Register (04)

