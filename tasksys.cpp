#include "tasksys.h"
#include <iostream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
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

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                          const std::vector<TaskID> &deps)
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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads), num_threads(num_threads)
{
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

// Static worker function for TaskSystemParallelSpawn
static void parallel_spawn_worker(IRunnable *runnable, int task_id, int num_total_tasks)
{
    runnable->runTask(task_id, num_total_tasks);
}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{
    std::vector<std::thread> threads;
    
    // Spawn one thread per task
    for (int i = 0; i < num_total_tasks; i++)
    {
        threads.push_back(std::thread(parallel_spawn_worker, runnable, i, num_total_tasks));
    }
    
    // Join all threads
    for (auto &t : threads)
    {
        t.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                 const std::vector<TaskID> &deps)
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
    : ITaskSystem(num_threads), num_threads(num_threads), runnable(nullptr), 
      num_total_tasks(0), next_task(0), shutdown(false)
{
    // Create thread pool
    for (int i = 0; i < num_threads; i++)
    {
        threads.push_back(std::thread(&TaskSystemParallelThreadPoolSpinning::worker_thread, this));
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning()
{
    // Signal shutdown
    {
        std::lock_guard<std::mutex> lock(task_lock);
        shutdown = true;
    }
    
    // Join all threads
    for (auto &t : threads)
    {
        t.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::worker_thread()
{
    while (true)
    {
        int task_id = -1;
        
        // Try to get next task
        {
            std::lock_guard<std::mutex> lock(task_lock);
            if (shutdown)
                break;
            if (next_task < num_total_tasks)
            {
                task_id = next_task++;
            }
        }
        
        // Execute task if available
        if (task_id >= 0)
        {
            runnable->runTask(task_id, num_total_tasks);
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks)
{
    this->runnable = runnable;
    this->num_total_tasks = num_total_tasks;
    this->next_task = 0;
    
    // Wait for all tasks to complete
    bool all_done = false;
    while (!all_done)
    {
        {
            std::lock_guard<std::mutex> lock(task_lock);
            all_done = (next_task >= num_total_tasks);
        }
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
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

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads), num_threads(num_threads), runnable(nullptr),
      num_total_tasks(0), next_task(0), shutdown(false), tasks_completed(0)
{
    // Create thread pool
    for (int i = 0; i < num_threads; i++)
    {
        threads.push_back(std::thread(&TaskSystemParallelThreadPoolSleeping::worker_thread, this));
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping()
{
    // Signal shutdown
    {
        std::lock_guard<std::mutex> lock(task_lock);
        shutdown = true;
    }
    
    // Wake up all threads
    worker_cv.notify_all();
    
    // Join all threads
    for (auto &t : threads)
    {
        t.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::worker_thread()
{
    while (true)
    {
        int task_id = -1;
        
        // Wait for task or shutdown signal
        {
            std::unique_lock<std::mutex> lock(task_lock);
            worker_cv.wait(lock, [this]() {
                return shutdown || next_task < num_total_tasks;
            });
            
            if (shutdown && next_task >= num_total_tasks)
                break;
                
            if (next_task < num_total_tasks)
            {
                task_id = next_task++;
            }
        }
        
        // Execute task if available
        if (task_id >= 0)
        {
            runnable->runTask(task_id, num_total_tasks);
            
            // Check if all tasks are done
            {
                std::lock_guard<std::mutex> lock(task_lock);
                tasks_completed++;
                if (tasks_completed == num_total_tasks)
                {
                    master_cv.notify_one();
                }
            }
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks)
{
    this->runnable = runnable;
    this->num_total_tasks = num_total_tasks;
    this->next_task = 0;
    this->tasks_completed = 0;
    
    // Wake up all worker threads
    worker_cv.notify_all();
    
    // Wait for all tasks to complete
    {
        std::unique_lock<std::mutex> lock(task_lock);
        master_cv.wait(lock, [this]() {
            return this->tasks_completed == this->num_total_tasks;
        });
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
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