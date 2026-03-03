#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <functional>

/*
 * TaskSystemSerial
 */
class TaskSystemSerial : public ITaskSystem
{
public:
    TaskSystemSerial(int num_threads);
    ~TaskSystemSerial();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();
};

/*
 * TaskSystemParallelSpawn
 */
class TaskSystemParallelSpawn : public ITaskSystem
{
public:
    TaskSystemParallelSpawn(int num_threads);
    ~TaskSystemParallelSpawn();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

    
private:
    int num_threads_;
};

/*
 * TaskSystemParallelThreadPoolSpinning
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem
{
public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);
    ~TaskSystemParallelThreadPoolSpinning();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int num_threads_;
    std::vector<std::thread> workers_;

    std::mutex mutex_;
    IRunnable *runnable_;
    int num_total_tasks_;
    std::atomic<int> next_task_;
    std::atomic<int> completed_tasks_;

    bool stop_;

    void workerThread();
};

/*
 * TaskSystemParallelThreadPoolSleeping
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem
{
public:
    TaskSystemParallelThreadPoolSleeping(int num_threads);
    ~TaskSystemParallelThreadPoolSleeping();
    const char *name();
    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int num_threads_;
    std::vector<std::thread> workers_;

    std::mutex mutex_;
    std::condition_variable worker_cv_;   // wake workers when tasks available
    std::condition_variable master_cv_;   // wake master when all done

    IRunnable *runnable_;
    int num_total_tasks_;
    int next_task_;
    int completed_tasks_;
    bool stop_;

    void workerThread();
};

#endif