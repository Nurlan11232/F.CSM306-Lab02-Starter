#include "tasksys.h"

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int /*num_threads*/) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name()
{
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads)
{
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks)
{
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable */*runnable*/, int /*num_total_tasks*/,
                                          const std::vector<TaskID> &/*deps*/)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name()
{
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads),num_threads_(num_threads)
{

    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}


void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{
    std::vector<std::thread>threads;
    threads.reserve(num_threads_);
    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    int actual_threads = std:: min(num_threads_, num_total_tasks);

    for (int t = 0; t < actual_threads; t++)
    {
        threads.emplace_back([t,actual_threads, num_total_tasks,runnable]()
    {
        for(int task_id= t; task_id< num_total_tasks; task_id +=actual_threads)
        {
            runnable->runTask(task_id,num_total_tasks);
        }
    });
    
    }
    for(auto&thread:threads){
        thread.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable */*runnable*/, int /*num_total_tasks*/,
                                                 const std::vector<TaskID> &/*deps*/)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name()
{
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads) 
      : ITaskSystem(num_threads),
        num_threads_(num_threads),
        runnable_(nullptr),
        num_total_tasks_(0),
        next_task_(0),
        tasks_done_(0),
        stop_(false),
        running_(false)
{
    for(int i = 0; i < num_threads_; i++)
        thread_pool_.emplace_back(&TaskSystemParallelThreadPoolSpinning::workerThread, this);
    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning()
{
    stop_.store(true);
    for(auto &th:thread_pool_)
        th.join();
}
void TaskSystemParallelThreadPoolSpinning::workerThread()
{
    while (!stop_.load())
    {
        if(!running_.load())
            continue;
        int task_id = -1;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if(next_task_ <num_total_tasks_)
                task_id = next_task_++;
        }
        if(task_id < 0){
            continue;
        }
        runnable_->runTask(task_id, num_total_tasks_);
        {
            std::lock_guard<std::mutex>lock(mtx_);
            tasks_done_++;
        }
    }
}
void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        runnable_        = runnable;
        num_total_tasks_ = num_total_tasks;
        next_task_      = 0;
        tasks_done_      = 0;
    }

    running_.store(true);

    while(true)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if(tasks_done_ == num_total_tasks)
            break;
    }

    running_.store(false);
    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    /*for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }*/
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable */*runnable*/, int /*num_total_tasks*/,
                                                              const std::vector<TaskID> &/*deps*/)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */


const char *TaskSystemParallelThreadPoolSleeping::name()
{
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads) \
      : ITaskSystem(num_threads),
        num_threads_(num_threads),
        runnable_(nullptr),
        num_total_tasks_(0),
        next_task_(0),
        tasks_done_(0),
        stop_(false)
{
    for(int i = 0; i < num_threads_; i++)
        thread_pool_.emplace_back(&TaskSystemParallelThreadPoolSleeping::workerThread, this);

    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping()
{
    {
        std::unique_lock<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_worker_.notify_all();
    for(auto &th : thread_pool_)
    th.join();    //
    // TODO: CSM306 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}
void TaskSystemParallelThreadPoolSleeping::workerThread()
{
    while(true)
    {
        std::unique_lock<std::mutex> lock(mtx_);

        cv_worker_.wait(lock, [this](){
            return stop_ || next_task_<num_total_tasks_;
        });
        if(stop_)return;

        int task_id = next_task_++;
        lock.unlock();

        runnable_->runTask(task_id, num_total_tasks_);

        lock.lock();
        tasks_done_++;

        if(tasks_done_ == num_total_tasks_){
            cv_main_.notify_one();
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks)
{
    {
        std::unique_lock<std::mutex> lock(mtx_);
        runnable_        = runnable;
        num_total_tasks_ = num_total_tasks;
        next_task_      = 0;
        tasks_done_      = 0;
    }

    cv_worker_.notify_all();

    std::unique_lock<std::mutex> lock(mtx_);
    cv_main_.wait(lock, [this](){
        return tasks_done_ == num_total_tasks_;
    });
    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    /*for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }*/
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable */*runnable*/, int /*num_total_tasks*/,
                                                              const std::vector<TaskID> &/*deps*/)
{

    //
    // TODO: CSM306 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{

    //
    // TODO: CSM306 students will modify the implementation of this method in Part B.
    //

    return;
}

const char *TaskSystemParallelThreadPoolSleepingAsync::name()
{
    return "Parallel + Thread Pool + Sleep + Async";
}

TaskSystemParallelThreadPoolSleepingAsync::TaskSystemParallelThreadPoolSleepingAsync(int n)
    : ITaskSystem(n), num_threads_(n), stop_(false),
    next_group_id_(0), active_groups_(0){
        for (int i = 0; i < n; i ++)
            thread_pool_.emplace_back(&TaskSystemParallelThreadPoolSleepingAsync::workerThread,this);

    }
TaskSystemParallelThreadPoolSleepingAsync::~TaskSystemParallelThreadPoolSleepingAsync(){
    {
        std::unique_lock<std::mutex> lk(mtx_); stop_ =true;
    }
    cv_worker_.notify_all();
    for(auto &th : thread_pool_) th.join();

    for(auto &kv : all_groups_ )delete kv.second;
}

void TaskSystemParallelThreadPoolSleepingAsync::promoteReadyGroups(){
    bool promoted = true;
    while (promoted ){
        promoted = false;
        for(auto it = waiting_list_.begin(); it !=waiting_list_.end();){
            TaskGroup *g = * it;
            bool all_done = true;
            for(TaskID dep_id : g ->deps){
                if(finished_ids_.find(dep_id )==finished_ids_.end()){
                    all_done = false;
                    break;
                }
            }
            if(all_done ){
                ready_queue_.push(g);
                it =waiting_list_.erase(it);
                promoted = true;
            }
            else{
                ++it;
            }
        }
    }
}

void TaskSystemParallelThreadPoolSleepingAsync::workerThread(){
    while (true){
        std::unique_lock<std::mutex> lk(mtx_);

        cv_worker_.wait(lk,[this](){
            return stop_ || !ready_queue_.empty();
        });

        if(stop_ )return;

        TaskGroup *g = ready_queue_.front();
        int task_id = g -> next_task++;
        

        if(g->next_task >= g->num_total_tasks_)
            ready_queue_.pop();

        lk.unlock();

        g->runnable ->runTask(task_id, g->num_total_tasks_);
        
        lk.lock();
        g->tasks_done++;

        if(g->tasks_done == g->num_total_tasks_){
            finished_ids_.insert(g->id);
            active_groups_--;

            promoteReadyGroups();

            if(active_groups_ == 0 ) 
            cv_main_.notify_one();

            cv_worker_.notify_all();
        }


    }
}

TaskID TaskSystemParallelThreadPoolSleepingAsync::runAsyncWithDeps(IRunnable*runnable, int num_total_tasks, const std::vector<TaskID> &deps)
{
    std::unique_lock<std::mutex> lk(mtx_);

    TaskGroup *g = new TaskGroup();
    g->id = next_group_id_++;
    g->runnable = runnable;
    g->num_total_tasks_= num_total_tasks;
    g->next_task = 0;
    g->tasks_done=0;

    for(TaskID dep : deps){
        if(finished_ids_.find(dep) == finished_ids_.end())
        g->deps.insert(dep);

    }

    all_groups_[g->id] = g;
    active_groups_++;

    if(g->deps.empty())
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
    cv_main_.wait(lk, [this, id](){
        return finished_ids_.find(id) != finished_ids_.end();
    });

}

void TaskSystemParallelThreadPoolSleepingAsync::sync(){
    std::unique_lock<std::mutex> lk(mtx_);
    cv_main_.wait(lk, [this](){return active_groups_ == 0;});
}