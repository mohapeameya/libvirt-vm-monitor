#  Cloud Management System(CMS) using the libvirt API

One Paragraph of project description goes here

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. <!-- See deployment for notes on how to deploy the project on a live system.-->

### Prerequisites
Clone the repository to get a copy for yourself
```
$ git clone https://github.com/mohapeameya/libvirt-vm-monitor.git
```
Change current directory to ```libvirt-vm-monitor```
```
$ cd libvirt-vm-monitor
```
Install QEMU/libvirt/KVM on your system. Check out the following links for instructions:
1. [KVM Installation for Ubuntu](https://help.ubuntu.com/community/KVM/Installation)
2. [How to Install and Configure KVM on Ubuntu 18.04 LTS Server](https://www.linuxtechi.com/install-configure-kvm-ubuntu-18-04-server/)

Install the following dependencies if not already installed:
```
$ sudo apt install qemu qemu-kvm -y
$ sudo apt install libvirt-daemon-system libvirt-clients libvirt-bin libvirt-dev -y
$ sudo apt install virt-top -y
$ sudo apt install bridge-utils virt-manager -y
$ sudo apt install libosinfo-bin -y
$ sudo apt install libguestfs-tools -y
$ sudo apt install python3 -y
$ sudo apt install python-tk -y
$ pip3 install python-libvirt matplotlib
```


### Installing
Start virt-manager to allow VM installation
```
$ sudo virt-manager
```
#### VM Installation
Install Ubuntu 18.04 LTS Server as Guest VM from ISO image using virt-manager as follows:
1. Select Ubuntu 18.04 LTS Server ISO image during installation
2. Allocate 2 GB RAM, 1 VCPU and 6GB Disk Space
4. Set Name: ubuntu1804-x  
where x is the instance # starting from 0
5. Network: virtual network'default': NAT
6. Network interface  
name: ens3  
type: eth  
IPv4 method: manual  
subnet: 192.168.122.0/24(subnet same as virbr0 interface on host)  
address: 192.168.122.y where y = x + 2 and x is the instance #  
gateway: 192.168.122.1  
name server: 192.168.122.1  
7. Use entire disk for installation
8. Set your server's name: ubuntu1804-x  
where x is the instance #
9. Finish installation of the VM and store its xml configuration file in host at  ```./xmlConfig/``` directory with the same naming convention(x is the instance # of the VM)
```
$ sudo virsh dumpxml ubuntu1804-x > ./xmlConfig/ubuntu1804-x.xml
```
#### Server Installation in VM 
To test the CMS, run a CPU intensive server in each VM instance as follows:
1. Create ```/etc/rc.local``` in the VM if not already present
```
$ sudo touch /etc/rc.local
```
2. Edit ```/etc/rc.local``` file using vim
```
$ sudo vim /etc/rc.local
```
3. If file is newly created, add the line below at start of the file
```
#!/bin/sh -e 
```
4. Add the following lines at the eof
```
IPADDR=`ifconfig ens3 | grep "inet " | awk -F'[: ]+' '{print $3}'`
sudo /root/server $IPADDR 55555 &
exit 0	
```

5. Give permissions if the file was created
```
$ sudo chmod +x /etc/rc.local
```

6. Create ```server.cpp``` in ```/root/```
```
$ sudo touch /root/server.cpp
```
7. Shutdown VM to enable safe file editing from host

8. Use virt-edit on host to edit server file ```/root/server.cpp``` and add content(x is the instance # of the VM):
```
$ sudo virt-edit -d ubuntu1804-x /root/server.cpp
```
9. Copy-paste server code into the VM's ```/root/server.cpp``` file and save changes

10. Install g++ on the VM
```
$ sudo apt install g++
```
11. Compile the server program
```
$ sudo g++ --std=c++11 -o /root/server /root/server.cpp -lpthread
```
12. Reboot the VM to enable the server  

Repeat VM and Server Installation steps for each instance of VM.

#### Client/Load-generator application
Compile the client/load-generator application on host 
```
$ g++ --std=c++11 -o loadgen loadgen.cpp -lpthread
```

## Running the test
1. Run the CMS application
```
$ python3 ./cms.py
```
2. Run any single VM instance through virt-manager as a default server, say instance 0(static IP 192.168.122.2). If you run multiple instances with no load on the VMs, CMS will shutdown all but 1 instance automatically.
3. Run client/load-generator application on host
```
$ ./loadgen <default_server_ip> <monitor_ip> <min_threads> <max_threads> <test_time_in_secs|time_per_thread_in_secs>
```
For example:
```
$ ./loadgen 192.168.122.2 127.0.0.1 15 50 5
```
4. As the client/load-generator increases/decreases the load on the VMs, CMS will automatically scale up/down the number of VMs without loss of any requests from the client/load-generator. One can confim auto-scaling behaviour using virt-manager.

5. Graphical visualization of test data
```
$ python3 ./plot.py
```

<!--Explain how to run the automated tests for this system-->

<!--### Break down into end to end tests-->

<!--Explain what these tests test and why-->

<!--### And coding style tests-->

<!--Explain what these tests test and why-->

<!-- ## Deployment-->

<!--Add additional notes about how to deploy this on a live system-->

## Built With

* [libvirt](https://libvirt.org/docs.html) - an open-source API, daemon and management tool for managing platform virtualization

<!--## Contributing-->

<!--Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.-->

<!--## Versioning-->

<!--We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). -->

## Authors

* **Ameya Mohape** - *Initial work* - [mohapeameya](https://github.com/mohapeameya)

<!--See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.-->

## License

This project is licensed under the GNU GENERAL PUBLIC LICENSE Version 3 - see the [LICENSE](LICENSE) file for details

<!--## Acknowledgments
* Hat tip to anyone whose code was used
* Inspiration
* etc-->
