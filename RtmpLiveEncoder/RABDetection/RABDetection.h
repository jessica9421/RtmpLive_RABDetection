#include "stdafx.h"
#include <cv.h>
#include <cxcore.h>
#include <cvaux.h>
#include <highgui.h>
#include <string.h>
#include <windows.h>
#include <iostream>
using namespace std;
///////////跟踪模块数据结构

const int MAX_STEP = 200;
typedef struct blockTrack
{
	bool flag;	//标识当前帧有无检测到前一帧跟踪的物体
	bool crsAear;	//是否在多边形内部	
	int step;	//记录运动步数
	CvPoint pt[MAX_STEP];	//记录运动路径
	CvRect rect;	//记录当前位置
}blockTrack;

//划分警戒区域变量
extern int t;	//点数组的下标
extern int a[][2];	//鼠标点击坐标数组
extern CvPoint **ptx;	//ptx[0]存点击的点的位置，因为cvPolyLine的调用需要用到指针的指针

///////初始化字体格式
extern CvFont font1;
extern CvPoint ptext;
extern int linetype;
extern string msg[];

extern const int CONTOUR_MAX_AERA ;
extern CvMemStorage *stor;
//CvFilter filter = CV_GAUSSIAN_5x5;
extern int nFrmNum ;	//当前帧数	
extern bool matchValue;
extern const int MAX_X_STEP;	//match距离
extern const int MAX_Y_STEP;	//match距离

extern const double MHI_DURATION;
extern const double MATCH_RATIO;
extern const int N;
extern IplImage **buf;
extern int last;
extern IplImage *mhi; // MHI: motion history image

extern std::vector<CvRect> objDetRect;	//矩形区域，保存当前帧检测到的物体的区域
extern std::vector<blockTrack> trackBlock;	 //根据物体对象	

extern void onMouse(int Event,int x,int y,int flags,void* param);
extern void m_Detect(IplImage* img, IplImage* dst, int diff_threshold);
extern bool match(CvRect rect1, CvRect rect2);
extern bool ptInPolygon(CvPoint** ptx,int t,CvPoint pt);