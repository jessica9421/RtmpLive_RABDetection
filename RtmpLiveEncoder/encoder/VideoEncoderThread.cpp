#include "stdafx.h"
#include "VideoEncoderThread.h"
#include "dshow/DSCapture.h"
#include "dshow/DSVideoGraph.h"
#include "X264Encoder.h"
#include "../Thread.h"
#include "RABDetection/RABDetection.h"
using namespace std;
#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))

VideoEncoderThread::VideoEncoderThread(DSVideoGraph* dsVideoGraph, int bitrate)
    : ds_video_graph_(dsVideoGraph), x264_encoder_(new X264Encoder)
    , is_save_picture_(false)
{
    // 初始化工作
    x264_encoder_->Initialize(ds_video_graph_->Width(), ds_video_graph_->Height(), 
        bitrate, ds_video_graph_->FPS());
    listener_ = NULL;
}

VideoEncoderThread::~VideoEncoderThread()
{
    delete x264_encoder_;
}

void VideoEncoderThread::Run()
{
    unsigned long yuvimg_size = ds_video_graph_->Width() * ds_video_graph_->Height() * 3 / 2;
    unsigned char* yuvbuf = (unsigned char*)malloc(yuvimg_size);
    unsigned char* x264buf = (unsigned char*)malloc(yuvimg_size*100);
    int x264buf_len = 0;

    __int64 last_tick = 0;
    unsigned int timestamp = 0;
    unsigned int last_idr_timestamp = 0;

    bool is_first = true;
    double cap_interval = 1000.0 / ds_video_graph_->FPS();

	char* rgbbuf = new char[320*240*3];
	IplImage* motion = NULL;
    while (false == IsStop())
    {
        //char* rgbbuf = ds_video_graph_->GetBuffer();
        unsigned int now_tick = ::GetTickCount();
        unsigned int next_tick = (unsigned int)(now_tick + cap_interval);
		
		int index = 0;
		HANDLE h[2] = {g_hFullSemaphore[index],g_hMutex[index]};
		if(WaitForMultipleObjects(2,h,TRUE,10)==WAIT_TIMEOUT)
			continue;
		
		CvPoint ptm;	//点
		int k = 0;
		objDetRect.clear();	//目标检测和跟踪向量，可转换成为数组
		trackBlock.clear();

		IplImage* imageresize = cvCreateImage(cvSize(320,240),IPL_DEPTH_8U,3);  
		IplImage* oldimage;
		oldimage = YV12_To_IplImage(images[index][out[index]],(unsigned char *)imagesData[index][out[index]],640,480);
		//oldimage->origin = 1;
		cvResize(oldimage,imageresize,CV_INTER_LINEAR);		//重新定义图像的大小成320×240
		images[index][out[index]] = imageresize;	

		cvNamedWindow("Motion", 0);	//初始化一个window
		cvShowImage("Motion",imageresize);	//在指定窗口名中展示图像，应该是展示第一帧的图像
		cvSetMouseCallback( "Motion", onMouse, 0 );	 //绑定点击事件，记录到a数组中
		cvWaitKey(100);	
	
		int objCross = false; //是否有物体进入

		ptx[0]=new CvPoint[t];	//把点击的位置存入ptx[0]
		for (;k<t;k++)
			ptx[0][k]=cvPoint(a[k][0],a[k][1]);

		if(true)	//从摄像头或者文件中抓取并返回一帧，进行处理
		{				
			if( !motion )	//cv中对图像的访问，创建处理图像motion
			{
				motion = cvCreateImage( cvSize(imageresize->width,imageresize->height), IPL_DEPTH_8U, 1 );	//创建头并分配数据
				cvZero( motion );	//矩阵清0	
				motion->origin = imageresize->origin;	
			}

//******跟踪目标初始化
			if (trackBlock.empty())	//如果跟踪为空，把上一帧检测到的物体rect保存为跟踪物体
			{
				for(vector<CvRect>::iterator it=objDetRect.begin(); it!=objDetRect.end();++it)	//检测到有物体，执行跟踪
				{
					blockTrack blockTemp;
					blockTemp.flag = false;	//是否在当前帧检测到
					blockTemp.crsAear = false;	
					blockTemp.pt[0] = cvPoint(it->x + it->width/2, it->y + it->height/2);	//初始位置，取矩形中心
					blockTemp.pt[MAX_STEP-1] = cvPoint(1, 1);  //最后位置
					blockTemp.rect = *it;	//位置rect
					blockTemp.step = 0;		//步数
					trackBlock.push_back(blockTemp);
				}
			}
			objDetRect.clear();		//把从上一帧检测到的物体清除

			m_Detect(imageresize,motion,30);    ////////////////////////////////运动目标检测主函数******

//******跟踪目标匹配
			for(vector<CvRect>::iterator it=objDetRect.begin(); it!=objDetRect.end();++it)	//当前帧检测到的物体
			{
				ptm=cvPoint(it->x+ it->width/2, it->y + it->height/2);	//取中心点
				for(vector<blockTrack>::iterator itt=trackBlock.begin(); itt!=trackBlock.end();++itt) //跟踪目标
				{
					if (itt->flag)
						continue;
					if(match(*it,itt->rect))  //做两重循环，匹配检测到的物体和之前跟踪到的物体，并更新跟踪物体信息
					{
						itt->rect = *it;
						itt->step = (itt->step+1)%MAX_STEP;	//如果超过了阈值则返回0记录
						itt->pt[itt->step] = ptm;	//更新路径
						itt->flag = true;	//在当前帧检测到
						matchValue = true;
						break;	//匹配成功，找到跟踪物体的新路径，跳出循环
					}
				}
				if (!matchValue)	//如果没有找到之前有跟踪的记录，创建新的跟踪记录
				{
					blockTrack temp;
					temp.flag = true;	//当前帧检测到
					temp.crsAear = false;	//在多边形内部
					temp.pt[0] = ptm;	//起点
					temp.pt[MAX_STEP-1] = cvPoint(1, 1);//终点	
					temp.rect = *it;
					temp.step = 0;
					trackBlock.push_back(temp);
				}
				matchValue = false;
			}

//******目标更新
			for(vector<blockTrack>::iterator ita=trackBlock.begin(); ita!=trackBlock.end();)	//跟踪物体循环
			{
				if(!ita->flag)	//在前一帧没有检测到该物体，去除检测物体					
					ita = trackBlock.erase(ita);
				else								
				{
					CvRect rect = ita->rect;
					ptm=cvPoint(rect.x+ rect.width/2, rect.y + rect.height/2);	//取得中心点
					cvRectangle(imageresize, cvPoint(rect.x,rect.y), 
					cvPoint(rect.x + rect.width, rect.y + rect.height),
					CV_RGB(255,255,0), 1, CV_AA,0);
					if ( ptInPolygon(ptx,t,ptm)||ita->crsAear ) //如果跟踪物体在之前画的多边形内部
					{
						ita->crsAear =true;
						cvRectangle(imageresize, cvPoint(rect.x,rect.y),	//画物体标识框
						cvPoint(rect.x + rect.width, rect.y + rect.height),
						CV_RGB(255,0,0), 1, CV_AA,0);
						if (ita->pt[MAX_STEP-1].x == 1)	 //画前一个路径到当前路径的线
							for(int i = 0;i<ita->step-1;i++)
								cvLine(imageresize,ita->pt[i],ita->pt[i+1],CV_RGB(255,0,0),1,8,0); 
						else 
							for(int i = 0;i<MAX_STEP-1;i++)
								cvLine(imageresize,ita->pt[(ita->step+i+1)%MAX_STEP],ita->pt[(ita->step+i+2)%MAX_STEP],CV_RGB(255,0,0),1,8,0); 

						cvPutText(imageresize, msg[0].c_str(), ptext, &font1, CV_RGB(255,0,0));	//打出alerm
						objCross = true;
					}

					ita->flag = false;
					ita++;
				}
			}	

			cvPolyLine(imageresize,ptx,&t,1,1,CV_RGB(255,255,0),1);      //画警戒区，根据ptx所有点画多边形
			cvShowImage("Motion",imageresize);	//展示图画
			cvWaitKey(1);	
		}

		cvReleaseImage(&oldimage);
		delete imagesData[index][out[index]];
		
		memcpy(rgbbuf,images[index][out[index]]->imageDataOrigin,320*240*3);
		
		cvReleaseImage(&images[index][out[index]]);	//此处记得释放，否则会造成内存泄漏
		out[index] = (out[index]+1)%SIZE_OF_BUFFER;
		ReleaseMutex(g_hMutex[index]);
		ReleaseSemaphore(g_hEmptySemaphore[index],1,NULL);

        if (/*ds_video_graph_->IsBufferAvailable() &&*/ rgbbuf)
        {
            if (last_tick == 0)
            {
                last_tick = ::GetTickCount();
            }
            else
            {
                __int64 nowtick = ::GetTickCount();
                if (nowtick < last_tick)
                    timestamp = 0;
                else
                    timestamp += (nowtick - last_tick);
                last_tick = nowtick;
            }

            bool is_keyframe = false;
            if (timestamp - last_idr_timestamp >= 3000 || timestamp == 0)
            {
                is_keyframe = true;
                last_idr_timestamp = timestamp;
            }

            if (is_save_picture_)
            {
                SaveRgb2Bmp(rgbbuf, ds_video_graph_->Width(),  ds_video_graph_->Height());
                is_save_picture_ = false;
            }

            RGB2YUV420_r((LPBYTE)rgbbuf, 320,  240,
                (LPBYTE)yuvbuf,  &yuvimg_size);

            x264buf_len = yuvimg_size*100;
            x264_encoder_->Encode(yuvbuf, x264buf, x264buf_len, is_keyframe);
            if (is_first)
            {
                listener_->OnSPSAndPPS(x264_encoder_->SPS(), x264_encoder_->SPSSize(),
                    x264_encoder_->PPS(), x264_encoder_->PPSSize());
                is_first = false;
            }

            if (x264buf_len > 0)
            {
                if (listener_ && x264buf_len)
                {
                    base::DataBuffer* dataBuf = new base::DataBuffer((char*)x264buf, x264buf_len);
                    listener_->OnCaptureVideoBuffer(dataBuf, timestamp, is_keyframe);
                }
            }
        }
     
        now_tick = ::GetTickCount();
        if (next_tick > now_tick)
        {
            Sleep(next_tick-now_tick);
        }
    }

	cvDestroyWindow("Motion");
    free(yuvbuf);
    free(x264buf);
}

void  VideoEncoderThread::SetOutputFilename(const std::string& filename)
{
    filename_264_ = filename;
}

bool VideoEncoderThread::RGB2YUV420(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len)
{
    int i, j; 
    unsigned char*bufY, *bufU, *bufV, *bufRGB,*bufYuv; 
    //memset(yuvBuf,0,(unsigned int )*len);
    bufY = yuvBuf; 
    bufV = yuvBuf + nWidth * nHeight; 
    bufU = bufV + (nWidth * nHeight* 1/4); 
    *len = 0; 
    unsigned char y, u, v, r, g, b,testu,testv; 
    unsigned int ylen = nWidth * nHeight;
    unsigned int ulen = (nWidth * nHeight)/4;
    unsigned int vlen = (nWidth * nHeight)/4; 
    for (j = 0; j<nHeight;j++)
    {
        bufRGB = RgbBuf + nWidth * (nHeight - 1 - j) * 3 ; 
        for (i = 0;i<nWidth;i++)
        {
            int pos = nWidth * i + j;
            r = *(bufRGB++);
            g = *(bufRGB++);
            b = *(bufRGB++);

            y = (unsigned char)( ( 66 * r + 129 * g + 25 * b + 128) >> 8) + 16 ;           
            u = (unsigned char)( ( -38 * r - 74 * g + 112 * b + 128) >> 8) + 128 ;           
            v = (unsigned char)( ( 112 * r - 94 * g - 18 * b + 128) >> 8) + 128 ;
            *(bufY++) = max( 0, min(y, 255 ));

            if (j%2==0&&i%2 ==0)
            {
                if (u>255)
                {
                    u=255;
                }
                if (u<0)
                {
                    u = 0;
                }
                *(bufU++) =u;
                //存u分量
            }
            else
            {
                //存v分量
                if (i%2==0)
                {
                    if (v>255)
                    {
                        v = 255;
                    }
                    if (v<0)
                    {
                        v = 0;
                    }
                    *(bufV++) =v;

                }
            }

        }

    }
    *len = nWidth * nHeight+(nWidth * nHeight)/2;
    return true;
}

bool VideoEncoderThread::RGB2YUV420_r(LPBYTE RgbBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len)
{
    int i, j; 
    unsigned char*bufY, *bufU, *bufV, *bufRGB; 
    //memset(yuvBuf,0,(unsigned int )*len);
    bufY = yuvBuf; 
    bufV = yuvBuf + nWidth * nHeight; 
    bufU = bufV + (nWidth * nHeight* 1/4); 
    *len = 0; 
    unsigned char y, u, v, r, g, b; 
    unsigned int ylen = nWidth * nHeight;
    unsigned int ulen = (nWidth * nHeight)/4;
    unsigned int vlen = (nWidth * nHeight)/4; 
    for (j = nHeight-1; j >= 0;j--)
    {
        bufRGB = RgbBuf + nWidth * (nHeight - 1 - j) * 3 ; 
        for (i = nWidth-1; i >= 0;i--)
        {
            //unsigned char* prgb = bufRGB + i*3;
            //int pos = nWidth * i + j;
            r = *(bufRGB++);
            g = *(bufRGB++);
            b = *(bufRGB++);

            y = (unsigned char)( ( 66 * r + 129 * g + 25 * b + 128) >> 8) + 16 ;           
            u = (unsigned char)( ( -38 * r - 74 * g + 112 * b + 128) >> 8) + 128 ;           
            v = (unsigned char)( ( 112 * r - 94 * g - 18 * b + 128) >> 8) + 128 ;
            *(bufY++) = max( 0, min(y, 255 ));

            if (j%2==0&&i%2 ==0)
            {
                if (u>255)
                {
                    u=255;
                }
                if (u<0)
                {
                    u = 0;
                }
                *(bufU++) =u;
                //存u分量
            }
            else
            {
                //存v分量
                if (i%2==0)
                {
                    if (v>255)
                    {
                        v = 255;
                    }
                    if (v<0)
                    {
                        v = 0;
                    }
                    *(bufV++) =v;

                }
            }

        }

    }
    *len = nWidth * nHeight+(nWidth * nHeight)/2;
    return true;
}

void VideoEncoderThread::SaveRgb2Bmp(char* rgbbuf, unsigned int width, unsigned int height)
{
    BITMAPINFO bitmapinfo;
    ZeroMemory(&bitmapinfo,sizeof(BITMAPINFO));
    bitmapinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapinfo.bmiHeader.biWidth = width;
    bitmapinfo.bmiHeader.biHeight = -height;
    bitmapinfo.bmiHeader.biPlanes = 1;
    bitmapinfo.bmiHeader.biBitCount =24;
    bitmapinfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapinfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapinfo.bmiHeader.biSizeImage = width*height;
    bitmapinfo.bmiHeader.biClrUsed = 0;        
    bitmapinfo.bmiHeader.biClrImportant = 0;

    BITMAPFILEHEADER bmpHeader;
    ZeroMemory(&bmpHeader,sizeof(BITMAPFILEHEADER));
    bmpHeader.bfType = 0x4D42;
    bmpHeader.bfOffBits = sizeof(BITMAPINFOHEADER)+sizeof(BITMAPFILEHEADER);
    bmpHeader.bfSize = bmpHeader.bfOffBits + width*height*3;

    FILE* fp = fopen("./CameraCodingCapture.bmp", "wb");
    if (fp)
    {
        fwrite(&bmpHeader, 1, sizeof(BITMAPFILEHEADER), fp);
        fwrite(&(bitmapinfo.bmiHeader), 1, sizeof(BITMAPINFOHEADER), fp);
        fwrite(rgbbuf, 1, width*height*3, fp);
        fclose(fp);
    }
}
