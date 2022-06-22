#include <iostream>
#include <unistd.h>
#include <vector>
#include <string.h>
#include "dhnetsdk.h"
#include "dhconfigsdk.h"
#include <stdio.h>
#include <time.h>

using namespace std;
static BOOL g_bNetSDKInitFlag = FALSE;
static LLONG g_lLoginHandle = 0L;
static LLONG attachTemperHandle = 0L;
static LLONG attachHandle = 0L;
static char g_szDevIp[32];
static WORD g_nPort = 37777;
static char g_szUserName[64];
static char g_szPasswd[64];
static short g_CmdSerial = 0;


//*********************************************************************************
//The callback is set by CLIENT_Init. When the device is offline, SDK will call this callback function.
void CALLBACK DisConnectFunc(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, DWORD dwUser);
//Set the callback function by CLIENT_SetAutoReconnect. When offline device is reconnected successfully, SDK will call the function.
void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);
void CALLBACK SnapRev(LLONG lLoginID, BYTE *pBuf, UINT RevLen, UINT EncodeType, DWORD CmdSerial, LDWORD dwUser);
void CALLBACK cbRadiometryAttachCB(LLONG lAttachHandle, NET_RADIOMETRY_DATA *pBuf, int nBufLen, LDWORD dwUser);
void CALLBACK cfRadiometryAttachTemperCB(LLONG lAttachTemperHandle, NET_RADIOMETRY_TEMPER_DATA *pBuf, int nBufLen, LDWORD dwUser);
int GetIntInput(char *szPromt, int &nError);
void GetStringInput(const char *szPromt, char *szBuffer);
//*********************************************************************************
void InitTest()
{
    // SDK initialization
    g_bNetSDKInitFlag = CLIENT_Init((fDisConnect)DisConnectFunc, (LDWORD)0);
    if (FALSE == g_bNetSDKInitFlag)
    {
        printf("Initialize client SDK fail; \n");
        return;
    }
    else
    {
        printf("Initialize client SDK done; \n");
    }
    // Get the SDK version information
    DWORD dwNetSdkVersion = CLIENT_GetSDKVersion();
    printf("NetSDK version is [%d]\n", dwNetSdkVersion);
    // Set reconnection callback. Internal SDK auto connects when the device disconnected.
    CLIENT_SetAutoReconnect(&HaveReConnect, 0);

    // Set device connection timeout and trial times.
    int nWaitTime = 5000; // Timeout is 5 seconds
    int nTryTimes = 3;    //If timeout, it will try to log in three times.
    CLIENT_SetConnectTime(nWaitTime, nTryTimes);

    NET_PARAM stuNetParm = {0};
    stuNetParm.nConnectTime = 3000; // The timeout of connection when login.
    CLIENT_SetNetworkParam(&stuNetParm);
    NET_IN_LOGIN_WITH_HIGHLEVEL_SECURITY stInparam;
    NET_OUT_LOGIN_WITH_HIGHLEVEL_SECURITY stOutparam;
    memset(&stInparam, 0, sizeof(stInparam));
    stInparam.dwSize = sizeof(stInparam);
    strncpy(stInparam.szIP, g_szDevIp, sizeof(stInparam.szIP) - 1);
    strncpy(stInparam.szPassword, g_szPasswd, sizeof(stInparam.szPassword) - 1);
    strncpy(stInparam.szUserName, g_szUserName, sizeof(stInparam.szUserName) - 1);
    stInparam.nPort = g_nPort;
    stInparam.emSpecCap = EM_LOGIN_SPEC_CAP_TCP;

    g_lLoginHandle = CLIENT_LoginWithHighLevelSecurity(&stInparam, &stOutparam);
    if (0 == g_lLoginHandle)
    {
        printf("CLIENT_LoginWithHighLevelSecurity %s[%d]Failed!Last Error[%x]\n", g_szDevIp, g_nPort, CLIENT_GetLastError());
    }
    else
    {
        printf("CLIENT_LoginWithHighLevelSecurity %s[%d] Success\n", g_szDevIp, g_nPort);
    }
    usleep(1 * 1000);
    printf("\n");
}

void RunTest()
{
    if (FALSE == g_bNetSDKInitFlag)
    {
        return;
    }
    if (0 == g_lLoginHandle)
    {
        return;
    }

    //Subscribe the temperature distribution data (heat map).
    NET_IN_RADIOMETRY_ATTACH stIn = {sizeof(stIn), 1, cbRadiometryAttachCB};
    NET_OUT_RADIOMETRY_ATTACH stOut = {sizeof(stOut)};
    attachHandle = CLIENT_RadiometryAttach(g_lLoginHandle, &stIn, &stOut, 3000);
    if (0 == attachHandle)
    {
        printf("%d", g_lLoginHandle);
    }
    //各エリア温度分布データを subscribe する
    NET_IN_RADIOMETRY_ATTACH_TEMPER sttIn = {sizeof(sttIn), 1, cfRadiometryAttachTemperCB};
    NET_OUT_RADIOMETRY_ATTACH_TEMPER sttOut = {sizeof(sttOut)};
    attachTemperHandle = CLIENT_RadiometryAttachTemper(g_lLoginHandle, &sttIn, &sttOut, 3000);
    if (0 == attachTemperHandle)
    {
        printf("%d", g_lLoginHandle);
    }

    //Set callback of video snapshot data
    CLIENT_SetSnapRevCallBack(SnapRev, 0);

    char szUserChoose[1];
    do
    {

        clock_t start, finsh;
        double totaltime;
        start = clock();
        time_t timep;
        struct tm *p;
        time(&timep);
        p = gmtime(&timep);
        clock_t t = clock();
        int ms = t * 1000 / CLOCKS_PER_SEC % 1000;
        //*********************************************************************************
        // Send out snapshot command to front-end device
        //Snapshot selection 0-normal snapshot mode 1-video snapshot mode 2-black screen mode
        int nSnapType = 0;
        SNAP_PARAMS stuSnapParams;
        stuSnapParams.Channel = 0;
        stuSnapParams.mode = nSnapType;
        // Ask for SN. The valid range is 0~65535, and the over range part will be cut off as unsigned short.
        stuSnapParams.CmdSerial++;
        //Send snapshot command to IPC
        if (FALSE == CLIENT_SnapPictureEx(g_lLoginHandle, &stuSnapParams))
        {
            printf("CLIENT_SnapPictureExFailed!LastError[%x]\n", CLIENT_GetLastError());
            return;
        }
        else
        {
            //printf("CLIENT_SnapPictureEx succ\n");
        }
        printf("picture = %d-%d-%d_%02d:%02d:%02d%03d\n", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec);
        //*********************************************************************************
        //inform to start acquiring heat map data,user malloc and free memory of pInParam and pOutParam
        NET_IN_RADIOMETRY_FETCH stInFetch = {sizeof(stInFetch), 1};
        NET_OUT_RADIOMETRY_FETCH stOutFetch = {sizeof(stOutFetch)};
        //Inform start of obtaining the heat map data
        CLIENT_RadiometryFetch(g_lLoginHandle, &stInFetch, &stOutFetch, 3000);
        printf("Temp_txt = %d-%d-%d_%02d:%02d:%02d%03d\n", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec);
        //*********************************************************************************
        NET_IN_RADIOMETRY_FETCH sttInFetch = {sizeof(sttInFetch), 1};
        NET_OUT_RADIOMETRY_FETCH sttOutFetch = {sizeof(sttOutFetch)};
        CLIENT_RadiometryFetch(g_lLoginHandle, &sttInFetch, &sttOutFetch, 3000);
        //*********************************************************************************
        stuSnapParams.Channel = 1;
        stuSnapParams.mode = nSnapType;
        stuSnapParams.CmdSerial++;
        //Send Tempsnapshot command to IPC
        if (FALSE == CLIENT_SnapPictureEx(g_lLoginHandle, &stuSnapParams))
        {
            printf("CLIENT_TempSnapPictureExFailed!LastError[%x]\n", CLIENT_GetLastError());
            return;
        }
        else
        {
            //printf("CLIENT_TempSnapPictureEx succ\n");
        }
        printf("picture_temp = %d-%d-%d_%02d:%02d:%02d%03d\n", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec);
        sleep(1);
        g_CmdSerial++;
        //*********************************************************************************
        finsh = clock();
        totaltime = (double)(finsh - start) / CLOCKS_PER_SEC;
        printf("Totaltime=%f\n", totaltime);
        GetStringInput("'q': exit; 'c': continue\n", szUserChoose);
    } while ('q' != szUserChoose[0]);
    return;
}

void EndTest()
{
    printf("input any key to quit!\n");
    getchar();

    if (0 != g_lLoginHandle)
    {
        //Log out of device
        if (FALSE == CLIENT_Logout(g_lLoginHandle))
        {
            printf("CLIENT_Logout Failed!Last Error[%x]\n", CLIENT_GetLastError());
        }
        else
        {
            g_lLoginHandle = 0;
            //Cancel subscribing the temperature distribution data
            CLIENT_RadiometryDetach(attachHandle);
            CLIENT_RadiometryDetachTemper(attachTemperHandle);
        }
    }
    if (TRUE == g_bNetSDKInitFlag)
    {
        //Release SDK resoure
        CLIENT_Cleanup();
        g_bNetSDKInitFlag = FALSE;
    }
    exit(0);
}

//*********************************************************************************

void CALL_METHOD DisConnectFunc(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, DWORD dwUser)
{
    printf("Call DisConnectFunc\n");
    printf("lLoginID[0x%x]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", dwUser);
    printf("\n");
}

void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
    printf("Call HaveReConnect\n");
    printf("lLoginID[0x%x]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", dwUser);
    printf("\n");
}

//Get and save snapshot info for further using by callback fSnapRev
void CALLBACK SnapRev(LLONG lLoginID, BYTE *pBuf, UINT RevLen, UINT EncodeType, DWORD CmdSerial, LDWORD dwUser)
{

    if (lLoginID == g_lLoginHandle)
    {
        if (NULL != pBuf && RevLen > 0)
        {
            char szPicturePath[256] = "";
            time_t rawtime;
            struct tm *info;
            time(&rawtime);
            info = localtime(&rawtime);
            char szTmpTime[128] = "";
            strftime(szTmpTime, sizeof(szTmpTime) - 1, "%Y-%m-%d %H:%M:%S", localtime(&rawtime));
            //put the normal-picture into the picture file
            if (CmdSerial & 1)
            {
                snprintf(szPicturePath, sizeof(szPicturePath) - 1, "picture/%d_%s.jpg", g_CmdSerial, szTmpTime);
                printf("szPicturePath: %s\n", szPicturePath);
                FILE *pFile = fopen(szPicturePath, "wb");
                if (NULL == pFile)
                {
                    return;
                }
                int nWrite = 0;
                while (nWrite != RevLen)
                {
                    nWrite += fwrite(pBuf + nWrite, 1, RevLen - nWrite, pFile);
                }
                fclose(pFile);
            }
            //put the Temperature-picture into the Temp_picture file
            else
            {
                snprintf(szPicturePath, sizeof(szPicturePath) - 1, "Temp_picture/%d_%s.jpg", g_CmdSerial, szTmpTime);
                printf("szPicturePath: %s\n", szPicturePath);
                FILE *pFile = fopen(szPicturePath, "wb");
                if (NULL == pFile)
                {
                    return;
                }
                int nWrite = 0;
                while (nWrite != RevLen)
                {
                    nWrite += fwrite(pBuf + nWrite, 1, RevLen - nWrite, pFile);
                }
                fclose(pFile);
            }
        }
    }
}
//State callback cbRadiometryAttachCB gets temperature distribution data
void CALLBACK cbRadiometryAttachCB(LLONG lAttachHandle, NET_RADIOMETRY_DATA *pBuf, int nBufLen, LDWORD dwUser)
{
    int nPixel = pBuf->stMetaData.nWidth * pBuf->stMetaData.nHeight;
    unsigned short *pGray = new unsigned short[nPixel];
    memset(pGray, 0, nPixel);
    float *pTemp = new float[nPixel];
    memset(pTemp, 0, nPixel);
    //Unzip the heat map data and convert it to the gray data and temperature data in the unit of pixel
    //pBuf-Heat map data
    //pGray-Unzipped data is a gray map.Introducing null pointer indicates this data is not needed
    //pTemp-Temperature data of each pixel.Introducing null pointer indicates this data is not needed
    CLIENT_RadiometryDataParse(pBuf, pGray, pTemp);
    char szPicturePath[256] = "";
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    float nMax;
    float nMix;
    nMax = pTemp[0];
    nMix = pTemp[0];
    char szTmpTime[128] = "";
    strftime(szTmpTime, sizeof(szTmpTime) - 1, "%Y-%m-%d %H:%M:%S", localtime(&rawtime));
    snprintf(szPicturePath, sizeof(szPicturePath) - 1, "date/%d_%s.txt", g_CmdSerial, szTmpTime);
    auto pFile = fopen(szPicturePath, "w");
    int max = 0, width = 256, height = 192;
    printf("TemptxtPath: %s\n", szPicturePath);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            fprintf(pFile, "%f", pTemp[i * width + j]);
            fprintf(pFile, ",");
        }

        fprintf(pFile, "\n");
    }
    fclose(pFile);
    delete[] pGray;
    delete[] pTemp;
}

void CALLBACK cfRadiometryAttachTemperCB(LLONG lAttachTemperHandle, NET_RADIOMETRY_TEMPER_DATA *pBuf, int nBufLen, LDWORD dwUser)
{
    printf("Every area temp\n");
    for (int i = 0; i < pBuf->nRadiometryTemperNum; i++)
    {
        int x, y;
        auto queryInfo = pBuf->stuRadiometryTemperInfo + i;
        printf("%s\n", queryInfo->szName);
        printf("ave:%f min:%f  max:%f ", queryInfo->stuQueryTemperInfo.fTemperAve,
               queryInfo->stuQueryTemperInfo.fTemperMin, queryInfo->stuQueryTemperInfo.fTemperMax);
        x = queryInfo->stuCoordinate->nx;
        y = queryInfo->stuCoordinate->ny;
        printf("Temp_X:%d Temp_y:%d\n",x,y);
        printf("x:%d y:%d\n", x / 8192 * 1280 + 320, y / 8192 * 960 + 60);
    }
}
//*********************************************************************************
int GetIntInput(char *szPromt, int &nError)
{
    long int nGet = 0;
    char *pError = NULL;
    printf(szPromt);
    char szUserInput[32] = "";
    scanf("%c", szUserInput);
    nGet = strtol(szUserInput, &pError, 10);
    if ('\0' != *pError)
    {

        nError = -1;
    }
    else
    {
        nError = 0;
    }
    return nGet;
}

void GetStringInput(const char *szPromt, char *szBuffer)
{
    printf(szPromt);
    scanf("%s", szBuffer);
}

int ipv4(char *ip)
{
    if (ip == NULL || ip[0] == '0' || ip[0] == '\0')
    {
        return -1;
    }

    for (int i = 0, count = 0; i < strlen(ip); i++)
    {
        if ((ip[i] != '.') && (ip[i] < '0' || ip[i] > '9'))
        {
            return -1;
        }
        if (ip[i] == '.')
        {
            count++;
            if (count > 3)
            {
                return -1;
            }
        }
    }

    int ip_num[4] = {-1, -1, -1, -1};
    char ip_s[4][4];
    memset(ip_s, 0, sizeof(char[4]) * 4);

    sscanf(ip, "%[^.].%[^.].%[^.].%[^ ]", ip_s[0], ip_s[1], ip_s[2], ip_s[3]);
    sscanf(ip_s[0], "%d", &ip_num[0]);
    sscanf(ip_s[1], "%d", &ip_num[1]);
    sscanf(ip_s[2], "%d", &ip_num[2]);
    sscanf(ip_s[3], "%d", &ip_num[3]);

    for (int i = 0; i < 4; i++)
    {
        if (strlen(ip_s[i]) == 0 || (ip_s[i][0] == '0' && ip_s[i][1] != '\0') || ip_num[i] < 0 || ip_num[i] > 255)
        {
            return -1;
        }
    }

    return 0;
}
int main(int argc, char *argv[])
{
    if (argc < 4 || argc > 4)
    {
        printf("please input 1.IP 2.Username 3.Password\n");
        return 0;
    }
    if (ipv4(argv[1]) != 0)
    {
        printf("please input 1.IP 2.Username 3.Password\n");
        return 0;
    }
    else
        strncpy(g_szDevIp, argv[1], 15);
    printf("%s\n", g_szDevIp);

    strncpy(g_szUserName, argv[2], 5);
    printf("%s\n", g_szUserName);

    strncpy(g_szPasswd, argv[3], 13);
    printf("%s\n", g_szPasswd);

    InitTest();
    RunTest();
    EndTest();
    return 0;
}
