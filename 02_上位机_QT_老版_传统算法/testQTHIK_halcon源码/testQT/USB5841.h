#ifndef _USB5841_DEVICE_
#define _USB5841_DEVICE_

#include<windows.h>

//***********************************************************
// 用于计数器CNT的参数结构
// 硬件参数ControlMode控制字模式选项
const long USB5841_GATEMODE_POSITIVE_0	= 0x00;		// COUNTER:GATE高电平时计数，低电平时停止计数，计数时重新写入初值，按初值计数
													// 计数结束产生中断：写入初值开始计数时OUT开始为0，当计数到0时OUT为1

const long USB5841_GATEMODE_RISING_1	= 0x01;		// COUNTER:GATE上边沿触发计数，计数中出现GATE上升沿重新装入初值计数
													// 可编程单拍脉冲：当写入初值时OUT为1，当开始计数时OUT为0，当计数到0时OUT再次为1

const long USB5841_GATEMODE_POSITIVE_2	= 0x02;		// COUNTER:GATE高电平时计数，低电平时停止计数，计数时重新写入初值，按初值计数
													// 频率发生器：计数期间OUT为1，计数到0后输出一个周期的0，并重新装入计数值计数

const long USB5841_GATEMODE_POSITIVE_3	= 0x03;		// COUNTER:GATE高电平时计数，低电平时停止计数，计数时重新写入初值，按初值计数
													// 方波发生器：计数期间OUT为1，计数到0后输出一个周期的0，并重新装入计数值计数

const long USB5841_GATEMODE_POSITIVE_4	= 0x04;		// COUNTER:GATE高电平时计数，低电平时停止计数，计数时重新写入初值，按初值计数
													// 软件触发选通：写入初值OUT为1， 计数结束OUT输出一个周期低电平信号

const long USB5841_GATEMODE_RISING_5	= 0x05;		// COUNTER:GATE上边沿触发计数，计数中出现GATE上升沿重新装入初值计数
													// 硬件触发选通：写入初值OUT为1， 计数结束OUT输出一个周期低电平信号

//***********************************************************
// CreateFileObject中的Mode参数使用的文件操作方式控制字(可通过或指令实现多种方式并操作)
const long	USB5841_modeRead			= 0x0000;   // 只读文件方式
const long  USB5841_modeWrite			= 0x0001;   // 只写文件方式
const long 	USB5841_modeReadWrite		= 0x0002;   // 既读又写文件方式
const long  USB5841_modeCreate			= 0x1000;   // 如果文件不存可以创建该文件，如果存在，则重建此文件，并清0

//***********************************************************
// 驱动函数接口
#ifndef _USB5841_DRIVER_
#define DEVAPI __declspec(dllimport)
#else
#define DEVAPI __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif
	//######################## 常规通用函数 #################################
	HANDLE DEVAPI FAR PASCAL USB5841_CreateDevice(int DeviceLgcID = 0); // 创建设备对象(该函数使用系统内逻辑设备ID）
	HANDLE DEVAPI FAR PASCAL USB5841_CreateDeviceEx(int DevicePhysID = 0); // 创建设备对象(该函数使用板卡物理设备ID）
	int DEVAPI FAR PASCAL USB5841_GetDeviceCount(HANDLE hDevice);      // 取得USB5841在系统中的设备数量
	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceCurrentID(HANDLE hDevice, PLONG DeviceLgcID, PLONG DevicePhysID); // 取得当前设备的逻辑ID号和物理ID号
	BOOL DEVAPI FAR PASCAL USB5841_ListDeviceDlg(void); // 用对话框列表系统当中的所有USB5841设备
	BOOL DEVAPI FAR PASCAL USB5841_ResetDevice(HANDLE hDevice);		 // 复位整个USB设备
    BOOL DEVAPI FAR PASCAL USB5841_ReleaseDevice(HANDLE hDevice);    // 设备句柄

	//##################### 计数器控制函数 ##########################
    BOOL DEVAPI FAR PASCAL USB5841_SetDeviceCNT(				// 初始化计数器
									HANDLE	hDevice,			// 设备句柄
									ULONG	ContrlMode,			// 计数器控制模式
									ULONG	CNTVal,				// 计数初值(32位)
									ULONG	ulChannel);			// 通道选择[0-2]			

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceCNT(				// 取得各路计数器的当前计数值
									HANDLE	hDevice,			// 设备对象句柄,它由CreateDevice函数创建
									PULONG	pCNTVal,			// 返回计数值(32位)
									ULONG	ulChannel);			// 通道选择[0-2]

	//############################ 开关量控制函数 ################################
	BOOL DEVAPI FAR PASCAL USB5841_EnableStsDIO(				// 设置开关量输入或输出状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BOOL bDir,					// bbDir控制DIO方向，TRUE：开关量输入，FALSE：开关量输出
									ULONG ulPort);				// DIO控制端口选择[0-5]，0代表DIO[7-0]，1代表DIO[15-8]，2代表DIO[23-16]，
																// 3代表DIO[31-24]，4代表DIO[39-32]，5代表DIO[47-40]
	// 对DIO[7-0]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PA(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PA(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)

	// 对DIO[15-8]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PB(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PB(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)

	// 对DIO[23-16]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PC(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PC(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)

	// 对DIO[31-24]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PD(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PD(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)

	// 对DIO[39-32]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PE(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PE(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)

	// 对DIO[47-40]操作
	BOOL DEVAPI FAR PASCAL USB5841_SetDeviceDO_PF(				// 输出开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDOSts[8]);			// bDOSts参数(注意: 必须定义为8个字节元素的数组)

	BOOL DEVAPI FAR PASCAL USB5841_GetDeviceDI_PF(				// 输入开关量状态
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									BYTE bDISts[8]);			// bDISts参数(注意: 必须定义为8个字节元素的数组)
	//############################################################################
	BOOL DEVAPI FAR PASCAL USB5841_GetDevVersion(				// 获取设备固件及程序版本
									HANDLE hDevice,				// 设备对象句柄,它由CreateDevice函数创建
									PULONG pulFmwVersion,		// 固件版本
									PULONG pulDriverVersion);	// 驱动版本

   	//########################## 文件操作函数 ####################################
    HANDLE DEVAPI FAR PASCAL USB5841_CreateFileObject(			// 创建文件对象
									HANDLE hDevice,				// 设备句柄,它应由CreateDevice函数创建
									LPCTSTR strFileName,		// 路径及文件名
									int Mode);					// 文件操作方式

    BOOL DEVAPI FAR PASCAL USB5841_WriteFile(					// 保存用户空间中数据到磁盘文件
									HANDLE hFileObject,			// 文件句柄,它应由CreateFileObject函数创建
									PVOID pDataBuffer,			// 用户数据空间地址
									LONG nWriteSizeBytes);		// 缓冲区大小(字节)

    BOOL DEVAPI FAR PASCAL USB5841_ReadFile(					// 从磁盘文件中读取数据到用户空间
									HANDLE hFileObject,			// 文件句柄,它应由CreateFileObject函数创建
									PVOID pDataBuffer,			// 接受文件数据的用户内存缓冲区
									LONG OffsetBytes,			// 从文件前端开始的偏移位置
									LONG nReadSizeBytes);		// 从偏移位置开始读的字节数

	BOOL DEVAPI FAR PASCAL USB5841_SetFileOffset(				// 设置文件偏移指针
									HANDLE hFileObject,			// 文件句柄,它应由CreateFileObject函数创建
									LONG nOffsetBytes);			// 文件偏移位置（以字为单位）  

	ULONGLONG DEVAPI FAR PASCAL USB5841_GetFileLength(HANDLE hFileObject); // 取得指定文件长度（字节）

    BOOL DEVAPI FAR PASCAL USB5841_ReleaseFile(HANDLE hFileObject);

	//############################ 线程操作函数 ################################
	DEVAPI HANDLE FAR PASCAL USB5841_CreateSystemEvent(void); 	// 创建内核系统事件对象
	BOOL DEVAPI FAR PASCAL USB5841_ReleaseSystemEvent(HANDLE hEvent); // 释放内核事件对象
	BOOL DEVAPI FAR PASCAL USB5841_CreateVBThread(HANDLE* hThread, LPTHREAD_START_ROUTINE RoutineAddr);
	BOOL DEVAPI FAR PASCAL USB5841_TerminateVBThread(HANDLE hThread);

	//################# 其他附加函数 ########################
	BOOL DEVAPI FAR PASCAL USB5841_kbhit(void); // 探测用户是否有击键动作(在控制台应用程序Console中且在非VC语言中)
	char DEVAPI FAR PASCAL USB5841_getch(void); // 等待并获取用户击键值(在控制台应用程序Console中有效)

#ifdef __cplusplus
}
#endif

// 自动包含驱动函数导入库
#ifndef _USB5841_DRIVER_
#ifndef _WIN64
#pragma comment(lib, "USB5841_32.lib")
#pragma message("======== Welcome to use our art company's products!")
#pragma message("======== Automatically linking with USB5841_32.dll...")
#pragma message("======== Successfully linked with USB5841_32.dll")
#else
#pragma comment(lib, "USB5841_64.lib")
#pragma message("======== Welcome to use our art company's products!")
#pragma message("======== Automatically linking with USB5841_64.dll...")
#pragma message("======== Successfully linked with USB5841_64.dll")
#endif

#endif

#endif; // _USB5841_DEVICE_