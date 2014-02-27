aravisGigE EPICS Support Module 
===============================

Introduction
------------

This module is a thin wrapper to the [aravis](http://live.gnome.org/Aravis) library for industrial gigabit ethernet cameras which provide a Genicam interface, and an epics driver is provided for any parameter exposed in a category of its genicam XML file. A python script is included for creating databases and edm screens from the XML file too.

Tested working cameras:

  * AVT Manta series
  * Prosilica GC series
  * Baumer TXG series
  * JAI Pulnix 6740 series

Tested mostly working cameras:

  * Sony CVC EH6300
    * 1080p/25 mode works best, still get occasional dropped frame
    * Using Zoom target seems to pause camera while zooming which then makes Aravis think the camera is disconnected. A reconnect fixes this
  * Point Grey Research Flea3 and Blackfly
    * Needs increased mem buffers as detailed under "Known Bugs"
    * Still get a number of dropped frames

The documentation here is top level documentation on creating an IOC with an aravisCamera areaDetector driver:

  * aravisCamera: areaDetector driver class
  * aravisCamera.template: Basic template that should be instantiated for any camera
  * Add-on templates with extra records for each camera type:
    * AVT_Manta.template
    * Prosilica_GC.template
    * Baumer_TXG.template
    * JAI_6740.template
    * PGR_Flea3.template
    * Sony_CVC_EH6300.template

If you need to add another type of similar camera, read the "Adding a new camera" section

Installation
------------

As this module is just a wrapper to aravis, it is not distributed with aravis, so you need to download and compile aravis in order to use the module. After downloading and untarring aravisGigE, either:

  * Run the bash script `install.sh` in the root of the module, or
  * Download and unzip the [aravis source](http://ftp.gnome.org/pub/GNOME/sources/aravis/0.1/aravis-0.1.15.tar.xz) in the vendor directory of the module, renaming it to aravis

You are now ready to build the module.

  * Modify configure/RELEASE.local
    * If you have glib >= 2.26 then comment out GLIBPREFIX, otherwise download and install glib and set the GLIBPREFIX macro to the install prefix you used. If you are installing 64-bit then please make sure you configure and install glib with the option --libdir=$GLIBPREFIX/lib64
    * Set AREADETECTOR to the path of areaDetector 1-6 or 1-7beta
  * Type make
  * Run bin/linux-x86/arv-tool-0.2 to see GigE visible devices
  * If your camera is not visible, check that it has an IP address on the same subnet
  * If your camera is of a supported type, modify the example to use the correct database and screen, otherwise read the next section

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
  * Once you have decided on the most useful features, copy them from <camera_name>-features.edl and paste them into <camera_name>.edl in order to create a useful summary screen

Known Bugs
----------

  * Changing resolution or colour mode while acquiring may produce dropped frames, aravisGigE will allow it and inserts a 1 second sleep to try and make this operation robust, but it is better to stop the camera before changing resolution or colour mode
  * `pthread_attr_setstacksize error Invalid argument` error messages are caused by glib and can be ignored
  * Please follow the section on sysctl in [this article](http://www.ptgrey.com/support/kb/index.asp?a=4&q=354) if you are getting large numbers of dropped frames
  * If you start the IOC without a camera connected, you will not be able to get all its features if you reconnect to it later. This may be fixed in a later release


Build Instructions for example
------------------------------
IOCs built using these build instructions are available in iocs/ 

Build Instructions for example 

* Add the dependencies to configure/RELEASE. 
<pre>
    ASYN=/dls_sw/prod/R3.14.12.3/support/asyn/4-21
    BUSY=/dls_sw/prod/R3.14.12.3/support/busy/1-4dls1
    AREADETECTOR=/dls_sw/prod/R3.14.12.3/support/areaDetector/1-9dls4
    ARAVISGIGE=/scratch/ad/aravisGigE
</pre>
* Add the DBD dependencies to src/Makefile 
<pre>
    example_DBD += base.dbd
    example_DBD += asyn.dbd
    example_DBD += busySupport.dbd
    example_DBD += ADSupport.dbd
    example_DBD += NDPluginSupport.dbd
    example_DBD += aravisCameraSupport.dbd
</pre>

* Add the LIBS dependencies to src/Makefile 
<pre>
    example_LIBS += aravisCamera
    example_LIBS += NDPlugin
    example_LIBS += ADBase
    example_LIBS += netCDF
    example_LIBS += cbfad
    example_LIBS += NeXus
    example_LIBS += hdf5
    example_LIBS += sz
    example_LIBS += PvAPI
    example_LIBS += GraphicsMagick++
    example_LIBS += GraphicsMagickWand
    example_LIBS += GraphicsMagick
    example_LIBS += busy
    example_LIBS += asyn
    example_SYS_LIBS += tiff
    example_SYS_LIBS += jpeg
    example_SYS_LIBS += z
    example_SYS_LIBS += gomp
    example_SYS_LIBS += X11
    example_SYS_LIBS += xml2
    example_SYS_LIBS += png12
    example_SYS_LIBS += bz2
    example_SYS_LIBS += Xext
    example_SYS_LIBS += freetype
</pre>
* Use the template files to add records to the database. 
<pre>
    # Macros:
    #  P        Device Prefix
    #  R        Device Suffix
    #  PORT     Asyn Port name
    #  TIMEOUT  Timeout
    #  ADDR     Asyn Port address
    file $(AREADETECTOR)/db/ADBase.template
    {
    pattern { P, R, PORT, TIMEOUT, ADDR }
        { "ARAVISCAM1", ":CAM:", "CAM1", "1", "0" }
    }
    
    # Macros:
    #  P        Device Prefix
    #  R        Device Suffix
    #  PORT     Asyn Port name
    #  TIMEOUT  Timeout
    #  ADDR     Asyn Port address
    file $(ARAVISGIGE)/db/aravisCamera.template
    {
    pattern { P, R, PORT, TIMEOUT, ADDR }
        { "ARAVISCAM1", ":CAM:", "CAM1", "1", "0" }
    }
    
    # Macros:
    #  P        Device Prefix
    #  R        Device Suffix
    #  PORT     Asyn Port name
    #  TIMEOUT  Timeout
    #  ADDR     Asyn Port address
    file $(ARAVISGIGE)/db/AVT_Manta_1_44_4.template
    {
    pattern { P, R, PORT, TIMEOUT, ADDR }
        { "ARAVISCAM1", ":CAM:", "CAM1", "1", "0" }
    }
    
    # Macros:
    #  P             Device Prefix
    #  R             Device Suffix
    #  PORT          Asyn Port name
    #  TIMEOUT       Timeout
    #  ADDR          Asyn Port address
    #  NDARRAY_PORT  Input Array Port
    #  NDARRAY_ADDR  Input Array Address
    #  Enabled       Plugin Enabled at startup?
    file $(AREADETECTOR)/db/NDPluginBase.template
    {
    pattern { P, R, PORT, TIMEOUT, ADDR, NDARRAY_PORT, NDARRAY_ADDR, Enabled }
        { "ARAVISCAM1", ":ARR:", "ARR1", "1", "0", "CAM1", "0", "1" }
    }
    
    # Macros:
    #  P          Device Prefix
    #  R          Device Suffix
    #  PORT       Asyn Port name
    #  TIMEOUT    Timeout
    #  ADDR       Asyn Port address
    #  TYPE       Asyn Type e.g. Int32
    #  FTVL       Format, e.g. Int
    #  NELEMENTS  Number of elements
    file $(AREADETECTOR)/db/NDStdArrays.template
    {
    pattern { P, R, PORT, TIMEOUT, ADDR, TYPE, FTVL, NELEMENTS }
        { "ARAVISCAM1", ":ARR:", "ARR1", "1", "0", "Int8", "UCHAR", "1442820" }
    }
</pre>
* Add the startup commands to st.cmd 
<pre>
    # Loading libraries
    # -----------------
    
    # Device initialisation
    # ---------------------
    
    cd "$(TOP)"
    
    dbLoadDatabase "dbd/example.dbd"
    example_registerRecordDeviceDriver(pdbbase)
    
    # aravisCameraConfig(portName, cameraName, maxBuffers, maxMemory)
    aravisCameraConfig("CAM1", "Allied Vision Technologies-50-0503332222", 50, -1)
    
    # NDStdArraysConfigure(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, maxMemory)
    NDStdArraysConfigure("ARR1", 2, 0, "CAM1", 0, 0)
</pre>

Example RELEASE.local
---------------------

Used at Diamond:

<pre>
SUPPORT=/dls_sw/prod/R3.14.12.3/support
WORK=/dls_sw/work/R3.14.12.3/support
EPICS_BASE=/dls_sw/epics/R3.14.12.3/base

GLIBPREFIX=/dls_sw/prod/tools/RHEL6-x86_64/glib/2-26-1/prefix

# ASYN is needed for base classes 
ASYN=$(SUPPORT)/asyn/4-22

# ADCORE is areaDetector
ADCORE=$(WORK)/ADCore
</pre>
