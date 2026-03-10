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
    std::cout << "Part I: Task System Benchmark" << std::endl;
    std::cout << "Threads: " << num_threads << ", Tasks: " << num_tasks << std::endl;
    std::cout << "========================================" << std::endl;

    ComputeTask task(num_tasks, workload_intensity);

    { ITaskSystem *s = new TaskSystemSerial(num_threads);
      runBenchmark(s, &task, num_tasks, "Serial System"); delete s; }

    { ITaskSystem *s = new TaskSystemParallelSpawn(num_threads);
      runBenchmark(s, &task, num_tasks, "Parallel Spawn"); delete s; }

    { ITaskSystem *s = new TaskSystemParallelThreadPoolSpinning(num_threads);
      runBenchmark(s, &task, num_tasks, "Parallel Spinning Pool"); delete s; }

    { ITaskSystem *s = new TaskSystemParallelThreadPoolSleeping(num_threads);
      runBenchmark(s, &task, num_tasks, "Parallel Sleeping Pool"); delete s; }

    return 0;
}
