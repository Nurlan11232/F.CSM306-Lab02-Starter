#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include<thread>
#include<vector>
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<queue>
#include<unordered_map>
#include<unordered_set>


class TaskSystemSerial : public ITaskSystem
{
    public :
        TaskSystemSerial (int num_threads);
        ~TaskSystemSerial();
        const char *name();
        void run (IRunnable *runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks, 
            const std::vector<TaskID> &deps);
        void sync();
};

class TaskSystemParallelSpawn : public ITaskSystem
{
    public:
        TaskSystemParallelSpawn (int num_threads);
        ~TaskSystemParallelSpawn();
        const char *name();
        void run (IRunnable *runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,const std::vector<TaskID> &deps);
        void sync();
        private:
    private:
        int num_threads_;
};
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem
{
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char *name();
        void run (IRunnable *runnable, int num_total_tasks);
        TaskID runAsyncWithDeps (IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps);
        void sync();
    private:
        int                       num_threads_;
        std::vector<std ::thread> thread_pool_;

        IRunnable                 *runnable_;
        int                       num_total_tasks_;
        int                       next_task_;
        int                       tasks_done_;

        std::mutex                mtx_;
        std::atomic<bool>         stop_;
        std::atomic<bool>         running_;

        void workerThread();
};
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem
{
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char *name();
        void run (IRunnable * runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable * runnable, int num_total_tasks, const std :: vector<TaskID> &deps);
        void sync();

    private:
        int                      num_threads_;
        std::vector<std::thread> thread_pool_;

        IRunnable                *runnable_;
        int                      num_total_tasks_;
        int                      next_task_;
        int                      tasks_done_;

        std::mutex               mtx_;
        std::condition_variable  cv_worker_;
        std::condition_variable  cv_main_;
        bool                     stop_;

        void workerThread();
};
struct TaskGroup{
    TaskID id;
    IRunnable *runnable;
    int num_total_tasks_;
    int next_task;
    int tasks_done;
    std::unordered_set<TaskID> deps;
};

#endif