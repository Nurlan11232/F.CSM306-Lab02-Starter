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

int main()
{
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    int workload_intensity = 200;

    std::cout << "========================================" << std::endl;
    std::cout << "Part II: Async with Dependencies" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "========================================" << std::endl;

    TaskSystemParallelThreadPoolSleepingAsync *s =
        new TaskSystemParallelThreadPoolSleepingAsync(num_threads);

    // Лаб-ын жишээ: A(128) -> B(2), C(6) -> D(32)
    ComputeTask tA(128, workload_intensity);
    ComputeTask tB(2,   workload_intensity);
    ComputeTask tC(6,   workload_intensity);
    ComputeTask tD(32,  workload_intensity);

    auto t0 = std::chrono::high_resolution_clock::now();

    TaskID idA = s->runAsyncWithDeps(&tA, 128, {});
    TaskID idB = s->runAsyncWithDeps(&tB, 2,   {idA});
    TaskID idC = s->runAsyncWithDeps(&tC, 6,   {idA});
    TaskID idD = s->runAsyncWithDeps(&tD, 32,  {idB, idC});

    s->sync();

    auto t1 = std::chrono::high_resolution_clock::now();

    std::cout << "Testing [Async A(128)->B(2),C(6)->D(32)]... Done. Time: "
              << std::fixed << std::setprecision(4)
              << std::chrono::duration<double>(t1 - t0).count() << "s" << std::endl;

    delete s;
    return 0;
}