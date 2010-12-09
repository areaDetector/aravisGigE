The Pulnix TMC1405GEV camera doesn't work with the default aravais acquisition protocol.

First extract the genicam xml file (I patched the aravis library, there may be a better way)

Recent wiresharks can parse the GigE Vision protocol.

decode.py reads a packet capture of the windows gui starting acqusition and decodes the register names:

GevCCPReg                     (0a00) = 00000002
GevCCPReg                     (0a00) = 00000002
GevHeartbeatTimeoutReg        (0938) = 00000bb8
GevMCTTReg                    (0b14) = 0000012c
GevMCRCReg                    (0b18) = 00000002
GevMCDAReg                    (0b10) = ac17059e
GevMCPHostPortReg             (0b00) = 000004db
GevSCDAReg                    (0d18) = ac17059e
GevSCPHostPortReg             (0d00) = 000004dc
AcquisitionStartReg           (d314) = 00000001

AcquisitionStopReg            (d318) = 00000001
GevMCPHostPortReg             (0b00) = 00000000
GevMCDAReg                    (0b10) = 00000000
GevSCPHostPortReg             (0d00) = 00000000
GevSCDAReg                    (0d18) = 00000000
GevMCPHostPortReg             (0b00) = 00000000
GevHeartbeatTimeoutReg        (0938) = 00000bb8
GevCCPReg                     (0a00) = 00000000

Then poke these settings into arvcameratest.c:

guint stream_port = arv_gv_stream_get_port (ARV_GV_STREAM (stream));

arv_device_write_register(arv_camera_get_device(camera), 0x0a00, 0x00000002);
arv_device_write_register(arv_camera_get_device(camera), 0x0938, 0x00000bb8);
arv_device_write_register(arv_camera_get_device(camera), 0x0b14, 0x0000012c);
arv_device_write_register(arv_camera_get_device(camera), 0x0b18, 0x00000002);
arv_device_write_register(arv_camera_get_device(camera), 0x0b10, 0xac17f435);
arv_device_write_register(arv_camera_get_device(camera), 0x0b00, 0x000004db);
arv_device_write_register(arv_camera_get_device(camera), 0x0d18, 0xac17f435);
arv_device_write_register(arv_camera_get_device(camera), 0x0d00, stream_port);
arv_device_write_register(arv_camera_get_device(camera), 0xd314, 0x00000001);


