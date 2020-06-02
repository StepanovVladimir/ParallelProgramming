#pragma once

#include "ITask.h"

class IWorker
{
public:
	virtual ~IWorker() = default;
	
	virtual bool ExecuteTask(ITask* task) = 0;
	virtual bool IsBusy() const = 0;
	virtual void Wait() const = 0;
};