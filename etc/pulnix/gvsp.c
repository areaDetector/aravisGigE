#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

enum
{
    HEADER = 1,
    FOOTER = 2,
    IMAGE  = 3
};

enum
{
    PACKET_FORMAT_OFS =  4,
    TSH_OFS           = 12,
    TSL_OFS           = 16,
    PIXEL_OFS         = 20,
    WIDTH_OFS         = 24,
    HEIGHT_OFS        = 28
};

enum
{
    MAX_PACKET = 2048
};

uint32_t ReadInt(char * buffer, int ofs)
{
    return ntohl(*((int *)(buffer + ofs)));
}

int ReadFrame(int socket, char * buffer, int count, int * width, int * height,
              uint64_t * timestamp)
{
    int ofs = 0;
    int headerSize = 8;
    int gotHeader = 0;
    char * packet = &buffer[count - MAX_PACKET];
    while(1)
    {
        int sz = read(socket, packet, MAX_PACKET);
        if(sz < 0)
        {
            return sz;
        }
        int packetformat = packet[PACKET_FORMAT_OFS];
        if(packetformat == HEADER)
        {
            *width = ReadInt(packet, WIDTH_OFS);
            *height = ReadInt(packet, HEIGHT_OFS);
            *timestamp = ReadInt(packet, TSH_OFS);
            *timestamp = *timestamp << 32 | ReadInt(packet, TSL_OFS);
            gotHeader = 1;
            ofs = 0;
        }
        else if(packetformat == IMAGE && gotHeader)
        {
            int payloadSize = sz - headerSize;
            if(ofs + payloadSize < count - MAX_PACKET)
            {
                memcpy(buffer + ofs, packet + headerSize, payloadSize);
                ofs += payloadSize;
            }
        }
        else if(packetformat == FOOTER && gotHeader)
        {
            return ofs;
        }
    }
    return ofs;
}
