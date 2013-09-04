#include "stdafx.h"
#include "Thread.h"
#include <windows.h>
#include "RGB.h"
#define _CRTDBG_MAP_ALLOC

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const unsigned short SIZE_OF_BUFFER = 100; //缓冲区长度
const unsigned short SIZE_OF_THREAD = 9;
bool g_continue[SIZE_OF_THREAD]; //控制程序结束

char * imagesData[SIZE_OF_THREAD][100];

IplImage* images[SIZE_OF_THREAD][100];//缓冲区是个循环队列
IplImage *back[SIZE_OF_THREAD];
unsigned short in[SIZE_OF_THREAD] = {0}; //产品进缓冲区时的缓冲区下标
unsigned short out[SIZE_OF_THREAD] = {0}; //产品出缓冲区时的缓冲区下标
HANDLE g_hMutex[SIZE_OF_THREAD]; //用于线程间的互斥
HANDLE g_hFullSemaphore[SIZE_OF_THREAD]; //当缓冲区满时迫使生产者等待
HANDLE g_hEmptySemaphore[SIZE_OF_THREAD]; //当缓冲区空时迫使消费者等待
IplImage* depthImages[SIZE_OF_THREAD][10];	//保存深度帧
HikPort g_CamOperate[SIZE_OF_THREAD];

float criteria[2]={0.8f,1.0f};//判断密度的阈值，这个在更换场景的时候修改

//HANDLE hThread[SIZE_OF_THREAD];
CWinThread* m_pThread[SIZE_OF_THREAD];

//int MSG[SIZE_OF_THREAD] ={WM_UPDATE_1,WM_UPDATE_2,WM_UPDATE_3,WM_UPDATE_4,WM_UPDATE_5,WM_UPDATE_6,WM_UPDATE_7,WM_UPDATE_8,WM_UPDATE_9};
char filename[SIZE_OF_THREAD][10]={"back1.jpg","back2.jpg","back3.jpg","back4.jpg","back5.jpg","back6.jpg","back7.jpg","back8.jpg","back9.jpg"};

typedef IplImage* (*lpAddFun)(IplImage*); //宏定义函数指针类型
typedef float (*CEFun)(IplImage* ,IplImage *,IplImage *,float ,bool,float *);
typedef void (*lpTopFun)(IplImage* m_depthImg, IplImage* m_colorImg, int* in_total, int* out_total, 
							CvBlobTrackerAuto* pTracker,  int oldBlob[], CvPoint oldBlobPoint[]); //宏定义函数指针类型

//针对特定的thread进行初始化
void init(int i)
{
	g_continue[i] = true;
	//创建各个互斥信号
	g_hMutex[i] = CreateMutex(NULL,FALSE,NULL);
	g_hEmptySemaphore[i] = CreateSemaphore(NULL,SIZE_OF_BUFFER-1,SIZE_OF_BUFFER-1,NULL);
	g_hFullSemaphore[i] = CreateSemaphore(NULL,0,SIZE_OF_BUFFER-1,NULL);
	in[i] = 0;
	out[i] = 0;
	back[i]=cvLoadImage(filename[i],1);
};

//YV12转为BGR24数据
bool YV12_To_BGR24(unsigned char* pYV12, unsigned char* pRGB24,int width, int height)
{
    if(!pYV12 || !pRGB24)
    {
        return false;
    }

    const long nYLen = long(height * width);
    const int halfWidth = (width>>1);

    if(nYLen<1 || halfWidth<1) 
    {
        return false;
    }

    unsigned char* yData = pYV12;
    unsigned char* vData = &yData[nYLen];
    unsigned char* uData = &vData[nYLen>>2];

    if(!uData || !vData)
    {
        return false;
    }

    // Convert YV12 to RGB24
    int rgb[3];
    int i, j, m, n, x, y;
    m = -width;
    n = -halfWidth;
    for(y=0; y<height;y++)
    {
        m += width;
            if(!(y % 2))
                n += halfWidth;
        for(x=0; x<width;x++)   
        {
            i = m + x;
                j = n + (x>>1);
			rgb[2] = yv2r_table[yData[i]][vData[j]];
			rgb[1] = yData[i] - uv2ig_table[uData[j]][vData[j]];
			rgb[0] = yu2b_table[yData[i]][uData[j]];
            //rgb[2] = int(yData[i] + 1.370705 * (vData[j] - 128)); // r
            //rgb[1] = int(yData[i] - 0.698001 * (uData[j] - 128)  - 0.703125 * (vData[j] - 128));   // g
            //rgb[0] = int(yData[i] + 1.732446 * (uData[j] - 128)); // b		
            
			//j = nYLen - iWidth - m + x;
            //i = (j<<1) + j;    //图像是上下颠倒的

            j = m + x;
            i = (j<<1) + j;

            for(j=0; j<3; j++)
            {
                if(rgb[j]>=0 && rgb[j]<=255)
                    pRGB24[i + j] = rgb[j];
                else
                    pRGB24[i + j] = (rgb[j] < 0)? 0 : 255;
            }
        }
    }

    if (pRGB24 == NULL)
    {
        return false;
    }

    return true;
}

IplImage* YV12_To_IplImage(IplImage* pImage,unsigned char* pYV12, int width, int height)
{
    if (!pYV12)
    {
        return NULL;
    }

    int sizeRGB = width* height *3;
    unsigned char* pRGB24 = new unsigned char[sizeRGB];
    memset(pRGB24, 0, sizeRGB);

    if(YV12_To_BGR24(pYV12, pRGB24 ,width, height) == false || (!pRGB24))
    {
        return NULL;
    }

    pImage = cvCreateImage(cvSize(width, height), 8, 3);
	pImage->origin = 1;
    if(!pImage)
    {
        return NULL;
    }

    memcpy(pImage->imageData, pRGB24, sizeRGB);
    if (!(pImage->imageData))
    {
        return NULL;
    }

    delete [] pRGB24;
    return pImage;
}
