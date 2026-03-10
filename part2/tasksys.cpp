#include "tasksys.h"

IRunnable::~IRunnable() {}
ITaskSystem::ITaskSystem(int /*num_threads*/) {}
ITaskSystem::~ITaskSystem() {}

const char *TaskSystemParallelThreadPoolSleepingAsync::name()
{
    return "Parallel + Thread Pool + Sleep + Async";
}

TaskSystemParallelThreadPoolSleepingAsync::TaskSystemParallelThreadPoolSleepingAsync(int n)
    : ITaskSystem(n), num_threads_(n), stop_(false),
      next_group_id_(0), active_groups_(0)
{
    for (int i = 0; i < n; i++)
        thread_pool_.emplace_back(
            &TaskSystemParallelThreadPoolSleepingAsync::workerThread, this);
}

TaskSystemParallelThreadPoolSleepingAsync::~TaskSystemParallelThreadPoolSleepingAsync()
{
    {
        std::unique_lock<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_worker_.notify_all();
    for (auto &th : thread_pool_) th.join();
    for (auto &kv : all_groups_) delete kv.second;
}

void TaskSystemParallelThreadPoolSleepingAsync::promoteReadyGroups()
{
    bool promoted = true;
    while (promoted) {
        promoted = false;
        for (auto it = waiting_list_.begin(); it != waiting_list_.end();) {
            TaskGroup *g = *it;
            bool all_done = true;
            for (TaskID dep_id : g->deps) {
                if (finished_ids_.find(dep_id) == finished_ids_.end()) {
                    all_done = false;
                    break;
                }
            }
            if (all_done) {
                ready_queue_.push(g);
                it = waiting_list_.erase(it);
                promoted = true;
            } else {
                ++it;
            }
        }
    }
}

void TaskSystemParallelThreadPoolSleepingAsync::workerThread()
{
    while (true) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_worker_.wait(lk, [this]() {
            return stop_ || !ready_queue_.empty();
        });
        if (stop_) return;

        TaskGroup *g = ready_queue_.front();
        int task_id = g->next_task++;
        if (g->next_task >= g->num_total_tasks_)
            ready_queue_.pop();
        lk.unlock();

        g->runnable->runTask(task_id, g->num_total_tasks_);

        lk.lock();
        g->tasks_done++;
        if (g->tasks_done == g->num_total_tasks_) {
            finished_ids_.insert(g->id);
            active_groups_--;
            promoteReadyGroups();
            if (active_groups_ == 0)
                cv_main_.notify_one();
            cv_worker_.notify_all();
        }
    }
}

TaskID TaskSystemParallelThreadPoolSleepingAsync::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps)
{
    std::unique_lock<std::mutex> lk(mtx_);

    TaskGroup *g = new TaskGroup();
    g->id = next_group_id_++;
    g->runnable = runnable;
    g->num_total_tasks_ = num_total_tasks;
    g->next_task = 0;
    g->tasks_done = 0;

    for (TaskID dep : deps) {
        if (finished_ids_.find(dep) == finished_ids_.end())
            g->deps.insert(dep);
    }

    all_groups_[g->id] = g;
    active_groups_++;

    if (g->deps.empty())
        ready_queue_.push(g);
    else
        waiting_list_.push_back(g);

    cv_worker_.notify_all();
    return g->id;
}

void TaskSystemParallelThreadPoolSleepingAsync::run(IRunnable *runnable, int num_total_tasks)
{
    TaskID id = runAsyncWithDeps(runnable, num_total_tasks, {});
    std::unique_lock<std::mutex> lk(mtx_);
    cv_main_.wait(lk, [this, id]() {
        return finished_ids_.find(id) != finished_ids_.end();
    });
}

void TaskSystemParallelThreadPoolSleepingAsync::sync()
{
    std::unique_lock<std::mutex> lk(mtx_);
    cv_main_.wait(lk, [this]() { return active_groups_ == 0; });
}