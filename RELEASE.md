aravasGigE Releases
===================

The latest untagged master branch can be obtained at
https://github.com/areaDetector/aravisGigE.

Tagged source code prior to release RX-Y are available via links at
http://controls.diamond.ac.uk/downloads/support/aravisGigE/

Tagged source code releases from R2-0 onward can be obtained at 
https://github.com/areaDetector/aravisGigE/releases.

Tagged prebuilt binaries from R2-0 onward can be obtained at
http://cars.uchicago.edu/software/pub/aravisGigE.

The versions of EPICS base, asyn, and other synApps modules used for each release can be obtained from 
the EXAMPLE_RELEASE_PATHS.local, EXAMPLE_RELEASE_LIBS.local, and EXAMPLE_RELEASE_PRODS.local
files respectively, in the configure/ directory of the appropriate release of the 
[top-level areaDetector](https://github.com/areaDetector/areaDetector) repository.


Release Notes
=============

R2-2 (30-June-2018)
----
* Requires areaDetector and ADCore R3-3-1
* Update to aravis 0.4.1
* Improved documentation in README
* aravisCamera.template
  * New records: MISSING_PKTS_RBV, PKT_RESEND, PKT_TIMEOUT, FRAME_RETENTION, HWIMAGEMODE, HWIMAGEMODE_RBV
  * Made ADDR=0 and TIMEOUT=1 be defaults
  * Added info tags for autosave
* Updated edl files for all cameras
* aravisGigEApp/src/Makefile
  * Simplified; user just sets GLIB_INCLUDE in CONFIG_SITE file; removed GLIBPREFIX code and merged GLIB_INC1 and GLIB_INC2 
    into GLIB_INCLUDE, because we now use addprefix to add the -I.
* iocs/aravisGigEIOC/aravisGigEApp/src/Makefile
  * Add additional libraries from glib when linking so it works with static builds


R2-1 (20-May-2016)
----
* First released version on github.
* Changes for compatibility with ADCore R2-2.
* Added support for PhotonFocus DR1
* Update to aravis 0.3.7
* Fix ROI size units to be binned pixels so it is consistent with ROI start

