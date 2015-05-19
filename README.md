# Introduction

uip-tools brings an easy way to upload or download files to your beloved TOS machine with NetUSBee compatible network adapter without a need to MiNT or Sting TCP/IP stacks.
It is a self contained binary, including a TCP/IP based on uIP embedded stack with DHCP support.

# User Interface

# Available REST API

In addition to the HTML based user interface, uiptool allows you to use a simple REST API to do a range of operations. All examples are provided as curl shell invocations. 

* Upload a file:

		curl -0T filename.tos 192.168.1.1/d/filename.tos

	Note that you need to specify not only destination folder but also a file name!

* Download a file:

		curl -0 192.168.1.1/c/filename.tos

* Run an executable:

		curl -0 192.168.1.1/c/filename.tos?run="command line"
	The executable needs to be already present on the recent machine.

* Delete a file:

		curl -0X DELETE 192.168.1.1/c/filename.tos

* Create a folder

		curl -0 192.168.1.1/c/foldername?newfolder

* Request file info or directory listing in json format

		curl -0 192.168.1.1/c/filename.tos?dir
		curl -0 192.168.1.1/c/foldername?dir