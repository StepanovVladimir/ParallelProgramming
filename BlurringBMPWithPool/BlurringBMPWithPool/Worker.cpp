#include "Worker.h"
#include "ThreadProc.h"
#include <iostream>

Worker::~Worker()
{
	WaitForSingleObject(_handle, INFINITE);
}

bool Worker::ExecuteTask(ITask* task)
{
	if (IsBusy())
	{
		return false;
	}

	_handle = CreateThread(NULL, 0, &ThreadProc, task, CREATE_SUSPENDED, NULL);
	ResumeThread(_handle);
	
	return true;
}

bool Worker::IsBusy() const
{
	if (_handle == nullptr)
	{
		return false;
	}

	LPDWORD code = new DWORD;
	GetExitCodeThread(_handle, code);

	if (*code == STILL_ACTIVE)
	{
		return true;
	}

	return false;
}

void Worker::Wait() const
{
	WaitForSingleObject(_handle, INFINITE);
}