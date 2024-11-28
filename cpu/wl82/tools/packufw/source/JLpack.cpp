#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>
#include <functional>
#include <string>
#include <mutex>
#include <memory>
#include <thread>
#include "JLpack.h"

using namespace std;

/*JLpack打包*/
/* JLpack.exe -f appUpdate.ufw -v 3.4.5 -f appUpdate.ufw -v 2.1.2  -o update.ufw  */

/*JLpack打包格式*/
// |Jlpack首头部| |输入文件1头部| |输入文件2头部| |其他输入文件头部...| |输入文件1数据| |输入文件2数据| |其他输入文件数据...|

//crc16
unsigned short CRC16(unsigned char* ptr, int len)
{
    unsigned short crc_ta[16] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    };
    unsigned short crc;
    unsigned char da;

    crc = 0;

    while (len-- != 0) {
        da = crc >> 12;
        crc <<= 4;
        crc ^= crc_ta[da ^ (*ptr / 16)];

        da = crc >> 12;
        crc <<= 4;
        crc ^= crc_ta[da ^ (*ptr & 0x0f)];
        ptr++;

    }

    return (crc);
}

void usageInfo(void)
{
    printf("usage : JLpack.exe -f appUpdate.ufw -v 3.4.5 -f appUpdate.ufw -v 2.1.2  -o update.ufw\n");
    printf("-f : Input file\n");
    printf("-v : File version, -f must be followed by -v\n");
    printf("-o : output file\n");
}

void printHeadinfo(JLFileHead *head)

{
    printf("---------------------------------------\n");
    printf("fileName : %s\n", head->fileName);
    printf("version : %s\n", head->version);
    printf("headCrc : 0x%x\n", head->headCrc);
    printf("dataCrc : 0x%x\n", head->dataCrc);
    printf("length : 0x%x\n", head->length);
    printf("address : 0x%x\n", head->address);
    printf("---------------------------------------\n");
}

int main(int argc, char* argv[])
{
    int i = 1;
    vector<string> inputNames;
    vector<string> inputFileVersion;
    JLFileHead Jlhead;
    char *outputFile = NULL;
    FILE *in_fd = NULL, *out_fd = NULL;
    unsigned int addressSeek = 0;

    while (i < argc) {
        if (strcmp(argv[i], "-f") == 0) {
            i++;

            if (i < argc)
            {
                inputNames.push_back(argv[i]);
                i++;

                //获取版本号
                if (strcmp(argv[i], "-v") == 0)
                {
                    i++;
                    inputFileVersion.push_back(argv[i]);
                    i++;
                }
                else {
                    exit(1);
                }
            }
            else { exit(1); }
        }
        else if (strcmp(argv[i], "-o") == 0) {
            i++;

            if (i < argc)
            {
                outputFile = argv[i];
                i++;
            }
            else { exit(1); }
        }
        else {
            //usageInfo();
            exit(1);
        }
    }

    if (inputNames.empty() || (NULL == outputFile))
    {
        usageInfo();
        exit(1);
    }

    //打开输出文件
    out_fd = fopen(outputFile, "wb");
    if (NULL == out_fd)
    {
        perror(outputFile);
        return -1;
    }

    //预留头部位置
    addressSeek = (inputNames.size() + 1) * sizeof(JLFileHead);
    //fseek(out_fd, addressSeek, SEEK_SET);

    for (int j = 0; j < inputNames.size(); j++)
    {
       FILE* fd = fopen(inputNames[j].c_str(), "rb");
       if (NULL == fd)
       {
           if (out_fd)
           {
               fclose(out_fd);
               remove(outputFile);
           }
           perror(inputNames[j].c_str());
           return -1;
       }else{
           JLFileHead fileHead;
           memset(&fileHead, 0, sizeof(fileHead));

           //文件名
           //memcpy(fileHead.fileName, inputNames[j].c_str(), strlen(inputNames[j].c_str() + 1));
           strcpy(fileHead.fileName, inputNames[j].c_str());

           //输入文件长度
           fseek(fd, 0, SEEK_END);
           fileHead.length = ftell(fd);
           fseek(fd, 0, SEEK_SET);

           //读取整个文件
           char* in_buf = new char[fileHead.length];
           fread(in_buf, 1, fileHead.length, fd);

           //计算数据crc
           unsigned int infile_crc = CRC16((unsigned char*)in_buf, fileHead.length);
           fileHead.dataCrc = infile_crc;

           //计算地址
           fileHead.address = addressSeek;

           //写数据
           fseek(out_fd, addressSeek, SEEK_SET);
           fwrite(in_buf, fileHead.length, 1, out_fd);
           addressSeek += fileHead.length;

           //写version
           strcpy(fileHead.version, inputFileVersion[j].c_str());

           //计算头部crc
           fileHead.headCrc = CRC16((unsigned char*)(&fileHead) + 2, sizeof(JLFileHead) - 2);

           //写头部
           fseek(out_fd, (j + 1) * sizeof(JLFileHead), SEEK_SET);
           fwrite(&fileHead, sizeof(JLFileHead), 1, out_fd);

           printHeadinfo(&fileHead);

           fclose(fd);
       }

    }

    //首头部
    strcpy(Jlhead.fileName, JL_PACK_FILE_NAME);
    Jlhead.length = addressSeek;
    Jlhead.fileNum = inputNames.size();
    Jlhead.headCrc = CRC16((unsigned char*)(&Jlhead) + 2, sizeof(JLFileHead) - 2);

    //写首头部
    fseek(out_fd, 0, SEEK_SET);
    fwrite(&Jlhead, sizeof(JLFileHead), 1, out_fd);
    printHeadinfo(&Jlhead);
    usageInfo();

    if (out_fd)
    {
        fclose(out_fd);
        out_fd = NULL;
    }
    
    return 0;
}