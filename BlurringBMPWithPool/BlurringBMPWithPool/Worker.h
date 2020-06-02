#pragma once

#include "IWorker.h"
#include <vector>

class Worker : public IWorker
{
public:
	~Worker();

	bool ExecuteTask(ITask* task) override;
	bool IsBusy() const override;
	void Wait() const override;

private:
	HANDLE _handle;
};