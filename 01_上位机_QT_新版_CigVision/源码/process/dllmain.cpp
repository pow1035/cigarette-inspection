// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "ImageProcess.h"
#include "paramStructs.h"
// 全局变量（如果需要）
static bool g_isInitialized = false;

/*
 * DLL 主入口函数
 * @param hModule      - DLL模块的句柄
 * @param ul_reason_for_call - DLL被调用的原因
 * @param lpReserved   - 保留参数，通常为NULL
 * @return TRUE表示成功，FALSE表示失败
 */
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // 当DLL被加载到进程中时调用
        // 在这里进行DLL的初始化工作
        g_isInitialized = false;
        break;
    case DLL_THREAD_ATTACH:
        // 当进程创建新线程时调用
        // 在这里进行线程相关的初始化
        break;
    case DLL_THREAD_DETACH:
        // 当线程正常退出时调用
        // 在这里进行线程相关的清理工作
        break;
    case DLL_PROCESS_DETACH:
        // 当DLL从进程中卸载时调用
        // 确保资源被正确释放
        if (g_isInitialized) {
            Cleanup();
        }
        break;
    }
    return TRUE;
}

