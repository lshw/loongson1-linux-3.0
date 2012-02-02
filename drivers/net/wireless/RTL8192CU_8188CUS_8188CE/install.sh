#!/bin/bash
# Writed by Realtek : willisTang
# September, 1 2010 v 1.0.0
###########################################################################
echo "		Auto install for 8192cu"
echo "		September, 1 2010 v 1.0.0"
cd driver
Drvfoulder=`ls |grep .tar.gz`
tar zxvf $Drvfoulder
Drvfoulder=`ls |grep -iv '.tar.gz'`
echo "$Drvfoulder"
cd  $Drvfoulder
echo "Authentication requested [root] for make driver:"
if [ "`uname -r |grep fc`" == " " ]; then
	sudo su -c make; Error=$?
else	
	su -c make; Error=$?
fi
module=`ls |grep -i 'ko'`
if [ "$Error" != 0 ];then
	echo "Compile make driver error: $Error, Please check error Mesg"
	read REPLY
	exit
else
	echo "Compile make driver ok!!"	
fi
if [ "`uname -r |grep fc`" == " " ]; then
	echo "Authentication requested [root] for remove driver:"
	sudo su -c "rmmod $module"
	echo "Authentication requested [root] for insert driver:"
	sudo su -c "insmod $module"
	echo "Authentication requested [root] for install driver:"
	sudo su -c "make install"
else
	echo "Authentication requested [root] for remove driver:"
	su -c "rmmod $module"
	echo "Authentication requested [root] for insert driver:"
	su -c "insmod $module"
	echo "Authentication requested [root] for install driver:"
	su -c "make install"
fi
echo "################################################################"
echo "The Setup Script is completed !"
echo "Plese Press any keyword to exit."
read REPLY
echo "################################################################"

