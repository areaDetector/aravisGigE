aravasGigE Releases
===================

The latest untagged master branch can be obtained at
https://github.com/areaDetector/aravisGigE.

Tagged source code prior to release RX-Y are available via links at
http://controls.diamond.ac.uk/downloads/support/aravisGigE/

Tagged source code releases from R2-0 onward can be obtained at 
https://github.com/areaDetector/aravisGigE/releases.

Tagged prebuilt binaries from R2-0 onward can be obtained at
https://cars.uchicago.edu/software/pub/aravisGigE.

The versions of EPICS base, asyn, and other synApps modules used for each release can be obtained from 
the EXAMPLE_RELEASE_PATHS.local, EXAMPLE_RELEASE_LIBS.local, and EXAMPLE_RELEASE_PRODS.local
files respectively, in the configure/ directory of the appropriate release of the 
[top-level areaDetector](https://github.com/areaDetector/areaDetector) repository.


Release Notes
=============

### R2-2 (XXX-July-2018)
----
* Requires areaDetector and ADCore R3-3-2
* Update to aravis 0.5.13.  
  * This version of aravis supports USB3 cameras in addition to GigE cameras.
  * aravisGigE now supports USB3 cameras as well, so the name is now a bit misleading.
    However, we don't plan to rename the driver or the repository.
* aravisCamera.cpp
  * Fixes so that it only calls the GigE specific functions if it is a GigE camera, not a USB3 camera.
  * Changed the logic for looking at differences between the set and readback values of float64 parameters.
    * Previously it checked that the fabs(value - rbv) > 0.001.
      This was failing when the values were large and the relative difference was still small.
      Now it looks for fabs((value - rbv)/value) > 0.001, i.e. it looks for 0.1% relative difference.
      It protects against divide by 0.
* Improved documentation in README
* Added NDDriverVersion and ADSDKVersion to driver. ADSDKVersion is the aravis release.
  Both of these must be manually updated for new releases of the driver or aravis.
* aravisCamera.template
  * New records: MISSING_PKTS_RBV, PKT_RESEND, PKT_TIMEOUT, FRAME_RETENTION, HWIMAGEMODE, HWIMAGEMODE_RBV
  * Made ADDR=0 and TIMEOUT=1 be defaults
  * Added info tags for autosave
* save/restore
  * Added aravisCamera_settings.req for manual method of auto_settings.req files
  * Added iocAravisGigE/auto_settings.req
* Updated edl files for all cameras
* Added src/makeAdl.py to read a GeniCAM XML file and create medm adl feature screens.
  It creates several reasonably sized screens, unlike the makeDbAndEdl.py which creates one very large edm screen.
* medm and autoconverted edm, caQtDM, and CSS-Boy screens
  * aravisTop.adl  
    * New top-level screen.  It loads aravisCamera.adl passing normal P and R macros but also
      C macro which is the name of the camera.  This allows aravisCamera.adl to load the camera-specific feature screens.
  * aravisCamera.adl  
    * Removed PVs not supported by the aravis driver
    * Added new PVs for ADCore 3-3.
    * Added related display widgets for camera-specific features screens.
    * Added related display widget for aravisMore.adl.
  * aravisMore.adl
    * New screen to for setup and statistics PVs
* Added op/Makefile to do autoconversions from adl to edl, opi, ui
* New autoconverted opi files with better medm file and better converters
* aravisGigEApp/src/Makefile
  * Simplified; user just sets GLIB_INCLUDE in CONFIG_SITE file; removed GLIBPREFIX code and merged GLIB_INC1 and GLIB_INC2 
    into GLIB_INCLUDE, because we now use addprefix to add the -I.
* iocs/aravisGigEIOC/aravisGigEApp/src/Makefile
  * Add additional libraries from glib when linking so it works with static builds
* TO DO BEFORE RELEASE:
  * Merge Michael Davidsaver's pull request?
  * Test with Oryx camera

### R2-1 (20-May-2016)
----
* First released version on github.
* Changes for compatibility with ADCore R2-2.
* Added support for PhotonFocus DR1
* Update to aravis 0.3.7
* Fix ROI size units to be binned pixels so it is consistent with ROI start

