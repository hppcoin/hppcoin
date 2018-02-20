High Performances Computing Network (HPP)
=========================================
Description
----------------------
High Performances Computing Platform (HPP) is an open source project launched by academic researchers and engineers to enable individuals, academic institutions and businesses to run High Performance Computing (HPC) tasks on a highly distributed platform based on the Blockchain Technology and open standards like [Open MPI](https://www.open-mpi.org) and [OpenCL](https://www.khronos.org/opencl/) with enterprise-grade scalability, security and reliability. The main purpose of HPP is the democratization of High Performave Computing infrastructures and the reduction of time-to-solution for HPC tasks with effective cost.
For more information visit the project website [hppcoin.org](http://hppcoin.org).

White Paper
----------------------
The [HPP whitepaper](doc/whitepaper.pdf) explains in details the High Performances Computing Plateform technology.

Roadmap
----------------------
The [HPP RoadMap](https://trello.com/b/w4CnFCdV/hppcoin) list all planned tasks by due time. This roadmap will be updated every time a new member or group join the HPP team.

Join the HPP Community
----------------------
We have an active [Slack](https://hppcoin.slack.com/) where  testers and users gather to discuss various aspects of HPP so feel free to ask any question or just say hello.

Contibuting to HPP
----------------------
The HPP project is still in an early developmental stage, we welcome every developer to join our team, just send an email to : team at hppcoin dot org and a team member will contact you as soon as possible. The HPP community has planned frequent development milestones based on a six month, time based release cycle.


Linux Build Instructions and Notes
==================================

Dependencies
----------------------
1.  Update packages

        sudo apt-get update

2.  Install required packagages

        sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev

3.  Install Berkeley DB 4.8

        sudo apt-get install software-properties-common
        sudo add-apt-repository ppa:bitcoin/bitcoin
        sudo apt-get update
        sudo apt-get install libdb4.8-dev libdb4.8++-dev

4.  Install QT 5

        sudo apt-get install libminiupnpc-dev libzmq3-dev
        sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev

Build
----------------------
1.  Clone the source:

        git clone https://github.com/hppcoin/hppcoin

2.  Build Hppcoin-core:

    Configure and build the headless hppcoin binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.
        
        ./autogen.sh
        ./configure
        make

3.  It is recommended to build and run the unit tests:

        make check


Mac OS X Build Instructions and Notes
=====================================
See (doc/build-osx.md) for instructions on building on Mac OS X.



Windows (64/32 bit) Build Instructions and Notes
=====================================
See (doc/build-windows.md) for instructions on building on Windows 64/32 bit.