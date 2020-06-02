#include "Pool.h"
#include "ThreadProc.h"

using namespace std;

Pool::Pool(vector<ITask*> tasks, unsigned threadsCount)
{
	_tasksCount = tasks.size();
	_handles = new HANDLE[_tasksCount];
	for (size_t i = 0; i < _tasksCount; i++)
	{
		_handles[i] = CreateThread(NULL, 0, &ThreadProc, tasks[i], CREATE_SUSPENDED, NULL);
	}
	_threadsCount = threadsCount;
}

void Pool::ExecuteTasks()
{
	unsigned count = 0;
	for (size_t i = 0; i < _tasksCount; i++)
	{
		ResumeThread(_handles[i]);
		count++;
		if (count == _threadsCount)
		{
			WaitForMultipleObjects(i + 1, _handles, true, INFINITE);
			count = 0;
		}
	}

	WaitForMultipleObjects(_tasksCount, _handles, true, INFINITE);
}