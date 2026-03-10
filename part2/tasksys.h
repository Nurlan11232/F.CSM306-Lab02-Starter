#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <unordered_set>

struct TaskGroup {
    TaskID id;
    IRunnable *runnable;
    int num_total_tasks_;
    int next_task;
    int tasks_done;
    std::unordered_set<TaskID> deps;
};

class TaskSystemParallelThreadPoolSleepingAsync : public ITaskSystem
{
public:
    TaskSystemParallelThreadPoolSleepingAsync(int num_threads);
    ~TaskSystemParallelThreadPoolSleepingAsync();
    const char *name();

    void run(IRunnable *runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                            const std::vector<TaskID> &deps);
    void sync();

private:
    int num_threads_;
    std::vector<std::thread> thread_pool_;

    std::mutex mtx_;
    std::condition_variable cv_worker_;
    std::condition_variable cv_main_;
    bool stop_;

    TaskID next_group_id_;
    std::queue<TaskGroup *> ready_queue_;
    std::vector<TaskGroup *> waiting_list_;
    std::unordered_map<TaskID, TaskGroup *> all_groups_;
    std::unordered_set<TaskID> finished_ids_;

    int active_groups_;

    void workerThread();
    void promoteReadyGroups();
};

#endif