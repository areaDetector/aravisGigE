aravisGigE EPICS Support Module 
===============================

Introduction
------------

This module is a thin wrapper to the [aravis](http://live.gnome.org/Aravis) library for industrial 
cameras the use the [GenICam standard](https://www.emva.org/standards-technology/genicam/).
These include 1Gbit Ethernet, 10Gbit Ethernet, and USB3 cameras and detectors. 
GenICam cameras contain an XML file that can be downloaded via a URL.  This XML file contains
a description of all of the features that the camera supports, organized in categories.
The aravisGigE driver can control any parameter exposed in this XML file. 
Python scripts are included for creating databases, edm screens, and medm screens from the XML file.

Tested working cameras:

  * AVT Manta series
  * Prosilica GC series
  * Baumer TXG series
  * JAI Pulnix 6740 series
  * Many more

Tested mostly working cameras:

  * Sony CVC EH6300
    * 1080p/25 mode works best, still get occasional dropped frame
    * Using Zoom target seems to pause camera while zooming which then makes Aravis think the camera is disconnected. A reconnect fixes this
  * Point Grey Research Flea3 and Blackfly
    * Needs increased mem buffers as detailed under "Known Bugs"
    * Still get a number of dropped frames

The following is a summary of the files included in aravisGigE:

  * aravisCamera.cpp: areaDetector driver class
  * aravisCamera.template: Basic template that should be instantiated for any camera
  * Add-on templates with extra records for each camera type:
    * AVT_Manta.template
    * Prosilica_GC.template
    * Baumer_TXG.template
    * JAI_6740.template
    * PGR_Flea3.template
    * Sony_CVC_EH6300.template
    * Many more
  * edl files
     * aravisCamera.edl: Base screen that gets included in a camera-specific screen
     * `<camera-name>.edl`: Python generated camera-specific top-level screen
     * `<camera-name>-features.edl`: Python generated camera-specific screen with camera features.  Can be very large.
  * adl files
     * aravisCamera.adl: Base screen that works with any camera
     * `<camera-name>-features_[1,2,3..].adl`: Python generated camera-specific screens with camera features. 
       Multiple screens are generated so they are not too large.
     * `<camera-name>-features.adl`  User-created screen that contains only the most important features.

If you need to add a new camera, read the "Adding a new camera" section

Installation
------------
This araviGigE repository module does not include the underlying aravis library.  However, it does include an install.sh script that downloads a version
of the aravis library into the aravisGigE/vendor subdirectory, and the top-level aravisGigE/Makefile builds the aravis library and tools, as well as
the EPICS aravisGigE library and example IOC.
  * Run the bash script `install.sh` in the top-level of aravisGigE.

You are now ready to build the module.

  * If you are using the standard areadDetector layout, with aravisGigE in a subdirectory of areaDetector, then edit the top-level areaDetector/configure files
  as described in [INSTALL_GUIDE.md](https://github.com/areaDetector/areaDetector/blob/master/INSTALL_GUIDE.md).
  * If you are not using this layout then you will need to edit aravisGigE/configure/RELEASE.local and aravisGigRE/configure/CONFIG_SITE to define things.
  * Type make
  * The first time make is run in the top-level aravisGigE directory it will build the aravis library and tools.  
    It will install aravisGigE/bin/linux-x86_64/arv-tool-VERSION , where VERSION is for example 0.6.
  * Run bin/linux-x86/arv-tool-VERSION to see GigE visible devices
  * If your camera is not visible, check that it has an IP address on the same subnet
  * If your camera is of a supported type, modify the example IOC to use the correct database and screen, otherwise read the section on adding a new camera.

System configuration
--------------------
In order to run Ethernet cameras without dropping frames it is usually necessary to increase a system buffer. 
This is particularly true if running multiple cameras from the same Linux machine.
The following instructions were copied from a [Point Grey/FLIR KnowledgeBase article](https://www.ptgrey.com/KB/10016).
```
Increase the amount of memory Linux uses for receive buffers using the sysctl interface. 
Whereas the system standard (default) and maximum values for this buffer default to 128 KB and 120 KB respectively, 
increasing both of these parameters to 1 MB significantly improves image streaming results.

Note: On some ARM boards, you may need to increase the receive buffer size to greater than 1 MB before noticing 
improved streaming results. Increasing the buffer size can enhance receive performance, but it also uses more memory.

The following sysctl command updates the receive buffer memory settings:

sudo sysctl -w net.core.rmem_max=1048576 net.core.rmem_default=1048576

Note: In order for these changes to persist after system reboots, the following lines must be manually added 
to the bottom of the /etc/sysctl.conf file:

net.core.rmem_max=1048576
net.core.rmem_default=1048576

Once changes are persisted, they can be reloaded at any time by running the following command in sysctl:

sudo sysctl -p

```

To use USB3 cameras on Linux, the [Point Grey/FLIR KnowledgeBase article 10016](https://www.ptgrey.com/KB/10016) is helpful.
It suggests the following:
```
To set the maximum usbfs memory limit until the next reboot, run this command:
$ sudo modprobe usbcore usbfs_memory_mb=1000


This method does not work with Ubuntu 14.04 or newer. With Ubuntu 14.04, users must set the memory limit permanently.

To set the maximum usbfs memory limit permanently:
1. Open the /etc/default/grub file in any text editor. Find and replace:

GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
with this:

GRUB_CMDLINE_LINUX_DEFAULT="quiet splash usbcore.usbfs_memory_mb=1000"

2. Update grub with these settings:

$ sudo update-grub

3. Reboot and test a USB 3.1 camera.

If this method fails to set the memory limit, run the following command:

$ sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'
To confirm that you have successfully updated the memory limit, run the following command:

cat /sys/module/usbcore/parameters/usbfs_memory_mb

```
In order for non-root users to access USB cameras on Linux a udev rule must be created to give read/write permission to
the selected users or groups.  Here is one way to do this:

1. Download the aravis.rules file from https://github.com/AravisProject/aravis/blob/master/aravis.rules.

2. Copy the aravis.rules file to /etc/udev/rules.d/.

3. Run `sudo udevadm control -R` to restart the udev daemon on RHEL/Centos.  The command may be different on other Linux systems.

4. If restarting the daemon is not sufficient then reboot the PC for the rules file to take effect.


Adding a new camera
-------------------
  * Run `bin/linux-x86/arv-tool-0.2 -n "<device_name>" genicam > "<camera_model>.xml"` to download the genicam xml data from the selected device cameras
    * E.g. `bin/linux-x86/arv-tool-0.2 -n "Allied Vision Technologies-50-0503318719" genicam > AVT_Manta_G125B.xml`
  * Make sure the generated file doesn't have a space at the start of it
  * Run `aravisGigEApp/src/makeDbAndEdl.py <genicam_xml> <camera_name>`
    * E.g. `aravisGigEApp/src/makeDbAndEdl.py AVT_Manta_G125B.xml AVT_Manta`
  * This should then create:
    * `aravisGigEApp/Db/<camera_name>.template`
    * `aravisGigEApp/opi/edl/<camera_name>.edl`
    * `aravisGigEApp/opi/edl/<camera_name>-features.edl`
  * Once you have decided on the most useful features, copy them from <camera_name>-features.edl and paste them into <camera_name>.edl 
    in order to create a useful summary screen
  * If you are using medm, or want to generate CSS-Boy or caQtDM screens from medm screens 
    then run `aravisGigEApp/src/makeAdl.py <genicam_xml> <camera_name>`.
    This will generate <camera_name>-features_1.adl through <camera_name>-features_N.adl, where N depends on the number of features and the
    maximum screen width and height set in makeAdl.py.
  * Once you have decided on the most useful features, copy them from <camera_name>-features_*.adl into a new file called <camera_name>-features.adl
    in order to create a useful summary screen.

Known Bugs
----------
  * Changing resolution or colour mode while acquiring may produce dropped frames, aravisGigE will allow it 
    and inserts a 1 second sleep to try and make this operation robust, 
    but it is better to stop the camera before changing resolution or colour mode
  * `pthread_attr_setstacksize error Invalid argument` error messages are caused by glib and can be ignored
  * If you start the IOC without a camera connected, you will not be able to get all its features if you reconnect to it later. 
    This may be fixed in a later release.


Build Instructions for example
------------------------------
IOCs built using these build instructions are available in iocs/ 

* If you are using the standard areadDetector layout, with aravisGigE in a subdirectory of areaDetector, then edit the top-level areaDetector/configure files
  as described in [INSTALL_GUIDE.md](https://github.com/areaDetector/areaDetector/blob/master/INSTALL_GUIDE.md).
* If you are not using this layout then you will need to edit the following files in aravisGigE/iocs/iocAravisGigE.  
  Exactly what to put in these files is beyond the scope of this document, since it depends on what plugins are to be used and where the
  support libraries for the plugins are located.
  * configure/RELEASE
  * src/Makefile
  * iocBoot/iocAravisGigE/st.cmd
