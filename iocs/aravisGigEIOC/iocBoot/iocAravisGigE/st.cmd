< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/aravisGigEApp.dbd")
aravisGigEApp_registerRecordDeviceDriver(pdbbase) 

epicsEnvSet("PREFIX", "13ARV1:")
epicsEnvSet("PORT",   "ARV1")
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "1920")
epicsEnvSet("YSIZE",  "2048")
epicsEnvSet("NCHANS", "2048")

#aravisCameraConfig("$(PORT)", "Prosilica-02-2131A-06202")
#aravisCameraConfig("$(PORT)", "Point Grey Research-14273040")
aravisCameraConfig("$(PORT)", "Photonic Science-V3")
asynSetTraceMask("$(PORT)",0,0x21)
dbLoadRecords("$(ADCORE)/db/ADBase.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ARAVISGIGE)/db/aravisCamera.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADCORE)/db/NDFile.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
#dbLoadRecords("$(ARAVISGIGE)/db/Prosilica_GC.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,CAMSERVER_PORT=camserver")
#dbLoadRecords("$(ARAVISGIGE)/db/PGR_Flea3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,CAMSERVER_PORT=camserver")
#dbLoadRecords("$(ARAVISGIGE)/db/PSL_SCMOS.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,CAMSERVER_PORT=camserver")
dbLoadRecords("$(ARAVISGIGE)/db/PSL_FDI3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,CAMSERVER_PORT=camserver")

# Create a standard arrays plugin
NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPluginBase.template","P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")
# Allow for cameras up to 2048x2048x3 for RGB
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=12582912")

# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd
set_requestfile_path("$(ADPILATUS)/prosilicaApp/Db")

#asynSetTraceMask("$(PORT)",0,255)
#asynSetTraceMask("$(PORT)",0,3)


iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30,"P=$(PREFIX)")
