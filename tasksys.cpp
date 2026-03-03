#include "tasksys.h"
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}

ITaskSystem::~ITaskSystem() {}


/* 
 * 1. Serial (Параллел биш)
 * 
 * Бүх task-ийг нэг thread ашиглан дарааллаар нь гүйцэтгэнэ.
 *  */

const char *TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads)
    : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
    // Task бүрийг дарааллаар нь ажиллуулна
    for (int i = 0; i < num_total_tasks; i++)
        runnable->runTask(i, num_total_tasks);
}

TaskID TaskSystemSerial::runAsyncWithDeps(
    IRunnable *runnable,
    int num_total_tasks,
    const std::vector<TaskID> &deps) {
    return 0; 
}

void TaskSystemSerial::sync() {
}


/* 
 * 2. Parallel Spawn
 * 
 * run() дуудагдах бүрт шинэ thread-үүд үүсгэнэ.
 * Task-уудыг thread бүрт статик байдлаар хуваана.
 * Сул тал: Thread үүсгэх устгах overhead их.
 *  */

const char *TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), num_threads_(num_threads) {}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {

    std::vector<std::thread> threads;
    threads.reserve(num_threads_);

    int tasks_per_thread =
        (num_total_tasks + num_threads_ - 1) / num_threads_;

    for (int t = 0; t < num_threads_; t++) {

        int start = t * tasks_per_thread;
        int end   = std::min(start + tasks_per_thread,
                             num_total_tasks);

        if (start >= num_total_tasks)
            break;

        threads.emplace_back([=]() {
            for (int i = start; i < end; i++)
                runnable->runTask(i, num_total_tasks);
        });
    }

    // Бүх thread дуусахыг хүлээнэ
    for (auto &th : threads)
        th.join();
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable *runnable,
    int num_total_tasks,
    const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemParallelSpawn::sync() {}


/* 
 * 3. Parallel Thread Pool + Spinning
 * 
 * Thread-үүд нэг удаа үүснэ.
 * Worker-ууд while(true) дотор spin хийж
 * task байгаа эсэхийг байнга шалгана.
 * Сул тал: CPU-г дэмий ашиглана (busy wait).
 *  */

const char *TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads),
      num_threads_(num_threads),
      runnable_(nullptr),
      num_total_tasks_(0),
      next_task_(0),
      completed_tasks_(0),
      stop_(false)
{
    // Worker thread-үүдийг үүсгэнэ
    for (int i = 0; i < num_threads_; i++)
        workers_.emplace_back(
            &TaskSystemParallelThreadPoolSpinning::workerThread,
            this);
}

TaskSystemParallelThreadPoolSpinning::
~TaskSystemParallelThreadPoolSpinning() {

    {
        std::lock_guard<std::mutex> lk(mutex_);
        stop_ = true;
        next_task_.store(num_total_tasks_);
    }

    for (auto &th : workers_)
        th.join();
}

void TaskSystemParallelThreadPoolSpinning::workerThread() {

    while (true) {

        if (stop_)
            return;

        // Дараагийн task-ийг atomic байдлаар авна
        int task_id = next_task_.fetch_add(1);

        int total;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            total = num_total_tasks_;
        }

        if (task_id < total) {

            IRunnable *r;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                r = runnable_;
            }

            if (r)
                r->runTask(task_id, total);

            completed_tasks_.fetch_add(1);
        }
        // else → spin үргэлжилнэ
    }
}

void TaskSystemParallelThreadPoolSpinning::run(
    IRunnable *runnable,
    int num_total_tasks) {

    {
        std::lock_guard<std::mutex> lk(mutex_);
        runnable_        = runnable;
        num_total_tasks_ = num_total_tasks;
        completed_tasks_.store(0);
        next_task_.store(0);
    }

    // Бүх task дуусах хүртэл busy wait
    while (completed_tasks_.load() < num_total_tasks) {
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                 const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {}


/* 
 * 4. Parallel Thread Pool + Sleeping
 * 
 * Worker-ууд task байхгүй үед унтана.
 * condition_variable ашиглан сэрээнэ.
 * CPU ашиглалт хамгийн үр ашигтай.
 *  */

const char *TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads),
      num_threads_(num_threads),
      runnable_(nullptr),
      num_total_tasks_(0),
      next_task_(0),
      completed_tasks_(0),
      stop_(false)
{
    for (int i = 0; i < num_threads_; i++)
        workers_.emplace_back(
            &TaskSystemParallelThreadPoolSleeping::workerThread,
            this);
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {

    {
        std::unique_lock<std::mutex> lk(mutex_);
        stop_ = true;
    }

    worker_cv_.notify_all();

    for (auto &th : workers_)
        th.join();
}

void TaskSystemParallelThreadPoolSleeping::workerThread() {

    while (true) {

        std::unique_lock<std::mutex> lk(mutex_);

        // Task байхгүй бол унтана
        worker_cv_.wait(lk, [this]() {
            return stop_ || next_task_ < num_total_tasks_;
        });

        if (stop_)
            return;

        int task_id = next_task_++;
        int total   = num_total_tasks_;
        IRunnable *r = runnable_;

        lk.unlock();

        r->runTask(task_id, total);

        lk.lock();

        completed_tasks_++;

        if (completed_tasks_ == total) {
            master_cv_.notify_one();
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run( IRunnable *runnable, int num_total_tasks) {

    {
        std::unique_lock<std::mutex> lk(mutex_);
        runnable_        = runnable;
        num_total_tasks_ = num_total_tasks;
        next_task_       = 0;
        completed_tasks_ = 0;
    }

    // Worker-уудыг сэрээнэ
    worker_cv_.notify_all();

    // Бүх task дуусахыг хүлээнэ
    {
        std::unique_lock<std::mutex> lk(mutex_);
        master_cv_.wait(lk, [this]() {
            return completed_tasks_ == num_total_tasks_;
        });
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable,int num_total_tasks,const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {}