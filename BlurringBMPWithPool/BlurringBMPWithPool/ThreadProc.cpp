#include "ThreadProc.h"

DWORD WINAPI ThreadProc(CONST LPVOID lpParam)
{
	ITask* task = (ITask*)lpParam;
	task->Execute();

	ExitThread(0);
}