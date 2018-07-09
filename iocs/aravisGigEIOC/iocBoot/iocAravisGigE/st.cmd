< envPaths
errlogInit(20000)

dbLoadDatabase("$(TOP)/dbd/aravisGigEApp.dbd")
aravisGigEApp_registerRecordDeviceDriver(pdbbase) 

# Prefix for all records
epicsEnvSet("PREFIX", "13ARV1:")
# The port name for the detector
epicsEnvSet("PORT",   "ARV1")
# The queue size for all plugins
epicsEnvSet("QSIZE",  "20")
# The maximim image width; used for row profiles in the NDPluginStats plugin
epicsEnvSet("XSIZE",  "1920")
# The maximim image height; used for column profiles in the NDPluginStats plugin
epicsEnvSet("YSIZE",  "2048")
# The maximum number of time series points in the NDPluginStats plugin
epicsEnvSet("NCHANS", "2048")
# The maximum number of frames buffered in the NDPluginCircularBuff plugin
epicsEnvSet("CBUFFS", "500")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

#aravisCameraConfig("$(PORT)", "Prosilica-02-2131A-06202")
#dbLoadRecords("$(ARAVISGIGE)/db/Prosilica_GC.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
#aravisCameraConfig("$(PORT)", "Point Grey Research-14273040")
#dbLoadRecords("$(ARAVISGIGE)/db/PGR_Flea3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
#aravisCameraConfig("$(PORT)", "Photonic Science-V3")
#dbLoadRecords("$(ARAVISGIGE)/db/PSL_SCMOS.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
#dbLoadRecords("$(ARAVISGIGE)/db/PSL_FDI3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
#aravisCameraConfig("$(PORT)", "Point Grey Research-17165235")
#dbLoadRecords("$(ARAVISGIGE)/db/PGR_BlackflyS_13Y3M.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
aravisCameraConfig("$(PORT)", "PointGrey-00EA4F2F")
dbLoadRecords("$(ARAVISGIGE)/db/PGR_GS3_U3_23S6M.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

asynSetTraceMask("$(PORT)",0,0x21)
dbLoadRecords("$(ARAVISGIGE)/db/aravisCamera.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

# Create a standard arrays plugin
NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, 0)
# Allow for cameras up to 2048x2048x3 for RGB
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int16,FTVL=SHORT,NELEMENTS=12582912")

# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd
set_requestfile_path("$(ADPILATUS)/prosilicaApp/Db")

#asynSetTraceMask("$(PORT)",0,255)
#asynSetTraceMask("$(PORT)",0,3)


iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30,"P=$(PREFIX)")
