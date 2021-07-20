#include "XVideoUI.h"
#include <QFileDialog>
#include <string>
#include <QMessageBox>
#include "XVideoThread.h"
#include "XFilter.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "XAudio.h"
#include <QFile>
using namespace std;
using namespace cv;
static bool pressSlider = false;
static bool isExport = false;
static bool isColor = true;
static bool isMark = false;
static bool isBlend = false;
static bool isMerge = false;

XVideoUI::XVideoUI(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint);
	qRegisterMetaType<cv::Mat>("cv::Mat");

	//原视频显示信号
	QObject::connect(XVideoThread::Get(),
		SIGNAL(ViewImage1(cv::Mat)),
		ui.src1,
		SLOT(SetImage(cv::Mat))
		);
	//原视频显示信号
	QObject::connect(XVideoThread::Get(),
		SIGNAL(ViewImage2(cv::Mat)),
		ui.src2,
		SLOT(SetImage(cv::Mat))
		);

	//输出视频显示信号
	QObject::connect(XVideoThread::Get(),
		SIGNAL(ViewDes(cv::Mat)),
		ui.des,
		SLOT(SetImage(cv::Mat))
		);


	//导出视频结束
	QObject::connect(XVideoThread::Get(),
		SIGNAL(SaveEnd()),
		this,
		SLOT(ExportEnd())
		);
	Pause();
	startTimer(40);
}
void XVideoUI::timerEvent(QTimerEvent *e)
{
	if (pressSlider)return;
	double pos = XVideoThread::Get()->GetPos();
	ui.playSlider->setValue(pos*1000);

}
void XVideoUI::Play()
{
	ui.pauseButton->show();
	ui.pauseButton->setGeometry(ui.playButton->geometry());
	XVideoThread::Get()->Play();
	ui.playButton->hide();
}
void XVideoUI::Pause()
{
	ui.playButton->show();
	//ui.playButton->setGeometry(ui.playButton->geometry());
	ui.pauseButton->hide();
	XVideoThread::Get()->Pause();
}
//打开视频源事件
void XVideoUI::Open()
{
	QString name = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("请选择视频文件"));
	if (name.isEmpty())return;
	string file = name.toLocal8Bit().data();
	if (!XVideoThread::Get()->Open(file))
	{
		QMessageBox::information(this, "error", name+" open failed!");
		return;
	}
		
	Play();
	//QMessageBox::information(this, "", name);
}

void XVideoUI::SliderPress()
{
	pressSlider = true;
}

void XVideoUI::SliderRelease()
{
	pressSlider = false;
}

void XVideoUI::Left(int pos)
{
	XVideoThread::Get()->SetBegin((double)pos / 1000.);
	SetPos(pos);
}

void XVideoUI::Right(int pos)
{
	XVideoThread::Get()->SetEnd((double)pos / 1000.);
}
//滑动条拖动
void XVideoUI::SetPos(int pos)
{
	XVideoThread::Get()->Seek((double)pos / 1000.);
}

//合并
void XVideoUI::Merge()
{
	isMark = false;
	isBlend = false;
	isMerge = false;
	QString name = QFileDialog::getOpenFileName(
		this, "select video:");
	if (name.isEmpty())
	{
		return;
	}
	std::string file = name.toLocal8Bit().data();
	isMerge = XVideoThread::Get()->Open2(file);
}
//融合
void XVideoUI::Blend()
{
	isMark = false;
	isBlend = false;
	isMerge = false;
	QString name = QFileDialog::getOpenFileName(
		this, "select video:");
	if (name.isEmpty())
	{
		return;
	}
	std::string file = name.toLocal8Bit().data();
	isBlend = XVideoThread::Get()->Open2(file);
}

//水印
void XVideoUI::Mark()
{
	isMark = false;
	isBlend = false;
	isMerge = false;
	QString name = QFileDialog::getOpenFileName(
		this, "select image:");
	if (name.isEmpty())
	{
		return;
	}
	std::string file = name.toLocal8Bit().data();
	cv::Mat mark = imread(file);
	if (mark.empty())return;
	XVideoThread::Get()->SetMark(mark);
	isMark = true;


}
//导出结束
void XVideoUI::ExportEnd()
{
	isExport = false;
	ui.exportButton->setText("Start Export");

	string src = XVideoThread::Get()->src1file;
	string des = XVideoThread::Get()->desFile;
	int ss = 0;
	int t = 0;
	ss = XVideoThread::Get()->totalMs * ((double)ui.left->value() / 1000.);
	int end = XVideoThread::Get()->totalMs * ((double)ui.right->value() / 1000.);
	t = end - ss;


	//处理音频
	XAudio::Get()->ExportA(src, src + ".mp3",ss,t);
	string tmp = des + ".avi";
	QFile::remove(tmp.c_str());
	QFile::rename(des.c_str(), tmp.c_str());
	XAudio::Get()->Merge(tmp, src + ".mp3", des);

	
	//XAudio::Get()->Merge()


}
//导出视频
void XVideoUI::Export()
{
	
	if (isExport)
	{
		//停止导出
		XVideoThread::Get()->StopSave();
		isExport = false;
		ui.exportButton->setText("Start Export");
		return;
	}
	//开始导出
	QString name = QFileDialog::getSaveFileName(
		this, "save", "out1.avi");
	if (name.isEmpty())return;
	std::string filename = name.toLocal8Bit().data();
	int w = ui.width->value();
	int h = ui.height->value();
	if (XVideoThread::Get()->StartSave(filename,w,h,isColor))
	{
		isExport = true;
		ui.exportButton->setText("Stop Export");
	}
}
//设置过滤器
void XVideoUI::Set()
{
	XFilter::Get()->Clear();
	isColor = true;
	//视频图像裁剪
	bool isClip = false;
	double cx = ui.cx->value();
	double cy = ui.cy->value();
	double cw = ui.cw->value();
	double ch = ui.ch->value();
	if (cx + cy + cw + ch > 0.0001)
	{
		isClip = true;
		XFilter::Get()->Add(XTask{ XTASK_CLIP, 
		{ cx,cy,cw,ch } });
		double w = XVideoThread::Get()->width;
		double h = XVideoThread::Get()->height;
		XFilter::Get()->Add(XTask{ XTASK_RESIZE, { w, h } });
	}


	//图像金字塔
	bool isPy = false;
	int down = ui.pydown->value();
	int up = ui.pyup->value();
	if (up > 0)
	{
		isPy = true;
		XFilter::Get()->Add(XTask{ XTASK_PYUP, { (double)up } });
		int w = XVideoThread::Get()->width;
		int h = XVideoThread::Get()->height;
		for (int i = 0; i < up; i++)
		{
			h = h * 2;
			w = w * 2;
		}
		ui.width->setValue(w);
		ui.height->setValue(h);
	}
	if (down > 0)
	{
		isPy = true;
		XFilter::Get()->Add(XTask{ XTASK_PYDOWN, { (double)down } });
		int w = XVideoThread::Get()->width;
		int h = XVideoThread::Get()->height;
		for (int i = 0; i < up; i++)
		{
			h = h / 2;
			w = w / 2;
		}
		ui.width->setValue(w);
		ui.height->setValue(h);
	}


	//调整视频尺寸
	double w = ui.width->value();
	double h = ui.height->value();
	if (!isMerge&&!isClip && !isPy&&ui.width->value() > 0 && ui.height->value() > 0)
	{
		XFilter::Get()->Add(XTask{ XTASK_RESIZE, { w, h } });
	}


	//对比度和亮度
	if (ui.bright->value() > 0 || 
		ui.contrast->value()>1)
	{
		XFilter::Get()->Add(XTask{ XTASK_GAIN,
		{ (double)ui.bright->value(), ui.contrast->value() } 
		});
	}

	//灰度图
	if (ui.color->currentIndex() == 1)
	{
		XFilter::Get()->Add(XTask{ XTASK_GRAY });
		isColor = false;
	}

	//图像旋转  1 90 2 180 3 270
	if (ui.rotate->currentIndex() == 1)
	{
		XFilter::Get()->Add(XTask{ XTASK_ROTATE90 });
	}
	else if (ui.rotate->currentIndex() == 2)
	{
		XFilter::Get()->Add(XTask{ XTASK_ROTATE180 });
	}
	else if (ui.rotate->currentIndex() == 3)
	{
		XFilter::Get()->Add(XTask{ XTASK_ROTATE270 });
	}


	//图像镜像
	if (ui.flip->currentIndex() == 1)
	{
		XFilter::Get()->Add(XTask{ XTASK_FLIPX });
	}
	else if (ui.flip->currentIndex() == 2)
	{
		XFilter::Get()->Add(XTask{ XTASK_FLIPY });
	}
	else if (ui.flip->currentIndex() == 3)
	{
		XFilter::Get()->Add(XTask{ XTASK_FLIPXY });
	}

	//水印
	if (isMark)
	{
		double x = ui.mx->value();
		double y = ui.my->value();
		double a = ui.ma->value();
		XFilter::Get()->Add(XTask{ XTASK_MARK, {x,y,a} });
	}


	//融合
	if (isBlend)
	{
		double a = ui.ba->value();
		XFilter::Get()->Add(XTask{ XTASK_BLEND, { a }});
	}

	//合并
	if (isMerge)
	{
		double h2 = XVideoThread::Get()->height2;
		double h1 = XVideoThread::Get()->height;
		int w = XVideoThread::Get()->width2 * (h2 / h1);
		XFilter::Get()->Add(XTask{ XTASK_MERGE });
		ui.width->setValue(XVideoThread::Get()->width + w);
		ui.height->setValue(h1);
	}
	
}