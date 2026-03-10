#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <thread>
#include "tasksys.h"

class ComputeTask : public IRunnable
{
public:
    std::vector<double> results;
    int workload_intensity;

    ComputeTask(int num_tasks, int intensity)
        : results(num_tasks, 0.0), workload_intensity(intensity) {}

    void runTask(int taskID, int num_total_tasks) override
    {
        double val = 0.0;
        for (int i = 0; i < workload_intensity; ++i)
            val += std::sin(i * 0.01 + taskID) * std::cos(i * 0.02 + taskID);
        results[taskID] = val;
    }
};

void runBenchmark(ITaskSystem *system, IRunnable *task, int num_tasks, const std::string &label)
{
    std::cout << "Testing [" << label << "]..." << std::flush;
    auto start = std::chrono::high_resolution_clock::now();
    system->run(task, num_tasks);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << " Done. Time: " << std::fixed << std::setprecision(4)
              << elapsed.count() << "s" << std::endl;
}

int main()
{
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    int num_tasks          = 5000;
    int workload_intensity = 200;

    std::cout << "========================================" << std::endl;
    std::cout << "Task System Benchmark" << std::endl;
    std::cout << "Threads: " << num_threads << ", Tasks: " << num_tasks << std::endl;
    std::cout << "========================================" << std::endl;

    ComputeTask task(num_tasks, workload_intensity);

    // 1. Serial System
    {
        ITaskSystem *s = new TaskSystemSerial(num_threads);
        runBenchmark(s, &task, num_tasks, "Serial System");
        delete s;
    }

    // 2. Parallel Spawn
    {
        ITaskSystem *s = new TaskSystemParallelSpawn(num_threads);
        runBenchmark(s, &task, num_tasks, "Parallel Spawn");
        delete s;
    }

    // 3. Parallel Spinning Pool
    {
        ITaskSystem *s = new TaskSystemParallelThreadPoolSpinning(num_threads);
        runBenchmark(s, &task, num_tasks, "Parallel Spinning Pool");
        delete s;
    }

    // 4. Parallel Sleeping Pool
    {
        ITaskSystem *s = new TaskSystemParallelThreadPoolSleeping(num_threads);
        runBenchmark(s, &task, num_tasks, "Parallel Sleeping Pool");
        delete s;
    }

    // ===== II ХЭСЭГ: Async + Dependencies =====
    std::cout << "\n========================================" << std::endl;
    std::cout << "Part II: Async with Dependencies" << std::endl;
    std::cout << "========================================" << std::endl;

    {
        TaskSystemParallelThreadPoolSleepingAsync *s =
            new TaskSystemParallelThreadPoolSleepingAsync(num_threads);

        // Лаб-ын жишээ: A(128) -> B(2), C(6) -> D(32)
        ComputeTask tA(128, workload_intensity);
        ComputeTask tB(2,   workload_intensity);
        ComputeTask tC(6,   workload_intensity);
        ComputeTask tD(32,  workload_intensity);

        auto t0 = std::chrono::high_resolution_clock::now();

        TaskID idA = s->runAsyncWithDeps(&tA, 128, {});         // A: хамааралгүй
        TaskID idB = s->runAsyncWithDeps(&tB, 2,   {idA});      // B: A дууссаны дараа
        TaskID idC = s->runAsyncWithDeps(&tC, 6,   {idA});      // C: A дууссаны дараа
        TaskID idD = s->runAsyncWithDeps(&tD, 32,  {idB, idC}); // D: B,C дууссаны дараа

        s->sync();

        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = t1 - t0;

        std::cout << "Testing [Async A(128)->B(2),C(6)->D(32)]..."
                  << " Done. Time: " << std::fixed << std::setprecision(4)
                  << elapsed.count() << "s" << std::endl;

        delete s;
    }

    return 0;
}