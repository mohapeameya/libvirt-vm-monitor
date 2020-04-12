#     CMS Application
#     Copyright (C) 2020  Ameya Mohape 
#     mohapeameya@gmail.com
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <https://www.gnu.org/licenses/>.

#!/usr/bin/python3

from __future__ import print_function
import sys, os
import libvirt
from time import sleep
import signal
import threading
import concurrent.futures

def keyboardInterruptHandler(signal, frame):
    print('KeyboardInterrupt received. Exiting...'.format(signal), file=sys.stdout)
    conn.close()
    exit(0)

def notifyClient():
	# other IPC mechanisms can be used to notify client
	# writing IPs of active domain to file to simplify communication with local client(load generator)
	f = open("./activeDomainInfo.txt", "w")

	# no need for a lock on activeDomainInfo as no other threads are running when this function is called
	for domainName in activeDomainInfo:
		domainIP = activeDomainInfo[domainName]
		f.write(domainIP + '\n')
	f.close()

def connectToHypervisor():
	conn = libvirt.open('qemu:///system')
	if conn == None:
	    print('Failed to open connection to qemu:///system', file=sys.stderr)
	    exit(1)
	return conn

def getDomainInfo(conn, domainNames):
	domainsInfo = {}
	for domainName in domainNames:
		domain = conn.lookupByName(domainName)
		domainsInfo[domainName] = domain
	return domainsInfo

def getDomainNames(conn):
	domainNames = conn.listDefinedDomains()
	if domainNames == None:
		print('Failed to get a list of domain names', file=sys.stderr)
	domainIDs = conn.listDomainsID()
	if domainIDs == None:
		print('Failed to get a list of domain IDs', file=sys.stderr)

	if len(domainIDs) != 0:
		for domainID in domainIDs:
			domain = conn.lookupByID(domainID)
			domainNames.append(domain.name())
	return domainNames

class CPUUsage:

	def __init__(self):
		self.guestTimeSum = 0.0
		self.activeVMCount = 0
		self.lock = threading.Lock()

	def checkCPUUsage(self, domainName, domainPtr):
		if domainPtr.isActive():
			stats1 = domainPtr.getCPUStats(True)		# assuming domain is still active
			sleep(samplingInterval)
			if domainPtr.isActive():
				stats2 = domainPtr.getCPUStats(True) 	# assuming domain is still active
				guestTime = (stats2[0]['cpu_time'] - (stats2[0]['system_time'] + stats2[0]['user_time'])) - (stats1[0]['cpu_time'] - (stats1[0]['system_time'] + stats1[0]['user_time']))
				# print('Thread running for {}'.format(domainName))
				with self.lock:
					# activeDomainInfo[domainName] = domainIPInfo[domainName]
					self.guestTimeSum = self.guestTimeSum + 100*guestTime/(1000000000.0*samplingInterval)
					self.activeVMCount = self.activeVMCount + 1
					activeDomainInfo[domainName] = domainIPInfo[domainName]
		else:
			with self.lock:
				if domainName in activeDomainInfo:
					activeDomainInfo.pop(domainName)

def spinUpVM(domainName):
	print('Spinning up VM...', file=sys.stdout)
	xmlFile = './xmlConfig/' + domainName + '.xml'
	xmlConfig = ''
	if os.path.exists(xmlFile):
		file = open(xmlFile,'r')
		xmlConfig = file.read()
		file.close()
	else:
		print('XML configuration file for ' + domainName + ' does not exist.')
		print('Error spinning up ' + domainName)
		return False
	domainPtr = conn.defineXML(xmlConfig)
	if domainPtr == None:
		print('Failed to define a domain from an XML definition.', file=sys.stderr)
		return False
	if domainPtr.create() < 0:
		print('Can not boot guest domain.', file=sys.stderr)
		return False
	return True


def spinDownVM(domainPtr):
	print('Spinning down VM...', file=sys.stdout)
	if domainPtr.shutdown() < 0:
		print('Can not shutdown guest domain.', file=sys.stderr)
		return False
	return True
	

def correctUtil(util):
	scaledUtil = util*scalingFactor
	if scaledUtil < 0:
		scaledUtil = scaledUtil*(-1)
	elif scaledUtil > 100.0:
		scaledUtil = 100.0
	return scaledUtil

# keyboard interrupt handler
signal.signal(signal.SIGINT, keyboardInterruptHandler)

# default hardcoded {domainName: IP} dictionary for 5 VMs
domainIPInfo = {'ubuntu1804-0': '192.168.122.2','ubuntu1804-1': '192.168.122.3','ubuntu1804-2': '192.168.122.4','ubuntu1804-3': '192.168.122.5','ubuntu1804-4': '192.168.122.6'}

# dictionry format {domainName: IP}
activeDomainInfo = {} 

# connect to hypervisor
conn = connectToHypervisor()

# get names of both active and inactive but persistent domains
domainNames = getDomainNames(conn)

# get virDomainPtr to all domains in domainNames
# dictionary format: {domainName:domainPtr}
domainsInfo = getDomainInfo(conn, domainNames)

vmCount = len(domainsInfo)
if vmCount <= 0:
	print('No VMs found. Exiting...', file=sys.stdout)
	exit(0)

samplingInterval = 10 										# sample cpu usage every 10 seconds 
scalingFactor = 1.0 										# cpu utilization values for guest from host and guest prespective are different. scalingFactor less than 1 (say .9) might be needed
scaleUpThreshold = 95.0									# threshold for spinning up VM
scaleDownThreshold = 85.0									# threshold for shutting down VM

# main loop
while True:

	cpuUsage = CPUUsage()

	# run threads to calculate cpu usage for all domains using ThreadPoolExecutor
	with concurrent.futures.ThreadPoolExecutor(max_workers=vmCount) as executor:
		for domainName in domainsInfo:
			executor.submit(cpuUsage.checkCPUUsage, domainName, domainsInfo[domainName])

	notifyClient()
	with cpuUsage.lock:		
		count = cpuUsage.activeVMCount
		_sum = cpuUsage.guestTimeSum

	if count <= 0:
		print('No VM running...')
		notifyClient()
		sleep(5)
		continue
	else:
		print('Active VM count: {}'.format(count), file=sys.stdout)
	util = _sum/count
	correctedUtil = correctUtil(util)
	
	print('Average CPU util : '+ str(correctedUtil), file=sys.stdout)
	print(activeDomainInfo)

	# overloaded servers
	if correctedUtil >= scaleUpThreshold:
		print('Overload detected')
		for domainName in domainsInfo:
			if domainsInfo[domainName].isActive() == False:
				spinUpStatus = False
				while domainsInfo[domainName].isActive() == False:
					spinUpStatus = spinUpVM(domainName)
					if spinUpStatus == False:			# VM failed to boot for some reason
						break
					sleep(5)							# wait for domain to begin boot before checking again
				if spinUpStatus == True:
					sleep(10)							# wait for domain to boot completely before updating activeDomainInfo dictionary
					print('VM ' + domainName + ' has booted', file=sys.stdout)
					activeDomainInfo[domainName] = domainIPInfo[domainName]	# add the VM to the active domain list
					notifyClient()						# notify client of the updated active domain list
					break								# get out of the loop so that no other VM is booted

	# underloaded servers
	if count > 1:
		if correctedUtil < (scaleDownThreshold*(count - 1))/count: # formula to balance N servers' load on N-1 servers
			print('Underload detected')
			for domainName in domainsInfo:
				if domainsInfo[domainName].isActive() == True:
					if domainName in activeDomainInfo:
						activeDomainInfo.pop(domainName)# remove domain from list of active domains
						notifyClient()					# notify client of the updated active domain list
					sleep(10)							# wait for client to get updated active domain list before spinning down VM
					spinDownStatus = False
					while domainsInfo[domainName].isActive() == True:		
						spinDownStatus = spinDownVM(domainsInfo[domainName])
						if spinDownStatus == False:		# VM failed to shutdown for some reason
							break
						sleep(5)						# wait for domain to shutdown before checking again
					if spinDownStatus == True:
						print('VM ' + domainName + ' has been shutdown.', file=sys.stdout)
						break							# get out of the loop so that no other VM is shutdown
					else:
						activeDomainInfo[domainName] = domainIPInfo[domainName] # add the VM to the active domain list that failed to shutwon
						notifyClient()					# notify client of the updated active domain list

	notifyClient() # notify client of the updated active domain list when VM is started or shutdown manually
	del cpuUsage
