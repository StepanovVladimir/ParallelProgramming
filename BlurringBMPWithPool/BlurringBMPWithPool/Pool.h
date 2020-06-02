#pragma once

#include "ITask.h"
#include <vector>

class Pool
{
public:
	Pool(std::vector<ITask*> tasks, unsigned threadsCount);

	void ExecuteTasks();

private:
	HANDLE* _handles;
	unsigned _tasksCount;
	unsigned _threadsCount;
};