#if defined (__cplusplus)
extern "C" {
#endif

#ifndef __JLPACK_H_
#define __JLPACK_H_

#define JL_PACK_FILE_NAME "EXT_UPDATE"

#define JLFILE_VERSION_LEN 32
typedef struct {
    unsigned short headCrc;
    unsigned short dataCrc;
    unsigned int   address;
    unsigned int   length;

    unsigned char u8Attribute;
    unsigned char res;
    unsigned short fileNum;
    char fileName[16];
    char version[JLFILE_VERSION_LEN];
} JLFileHead;

unsigned short CRC16(unsigned char* ptr, int len);

#endif /* __JLPACK_H_ */

#if defined (__cplusplus)
}
#endif

