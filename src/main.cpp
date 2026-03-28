#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include "configreader.h"
#include "process.h"
#include <algorithm>

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex queue_mutex;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data);
void printProcessOutput(std::vector<Process*>& processes);
std::string makeProgressString(double percent, uint32_t width);
uint64_t currentTime();
std::string processStateToString(Process::State state);

int main(int argc, char *argv[])
{
    // Ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Declare variables used throughout main
    int i;
    SchedulerData *shared_data = new SchedulerData();
    std::vector<Process*> processes;

    // Read configuration file for scheduling simulation
    SchedulerConfig *config = scr::readConfigFile(argv[1]);

    // Store number of cores in local variable for future access
    uint8_t num_cores = config->cores;

    // Store configuration parameters in shared data object
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    // Create processes
    uint64_t start = currentTime();
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        // If process should be launched immediately, add to ready queue
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
        }
    }

    // Free configuration data from memory
    scr::deleteConfig(config);

    // Launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

    // Main thread work goes here
    initscr();
    while (!(shared_data->all_terminated))
    {
        // Do the following:
        //   - Get current time
        //   - *Check if any processes need to move from NotStarted to Ready (based on elapsed time), and if so put that process in the ready queue
        //   - *Check if any processes have finished their I/O burst, and if so put that process back in the ready queue
        //   - *Check if any running process need to be interrupted (RR time slice expires or newly ready process has higher priority)
        //     - NOTE: ensure processes are inserted into the ready queue at the proper position based on algorithm
        //   - Determine if all processes are in the terminated state
        //   - * = accesses shared data (ready queue), so be sure to use proper synchronization
        
        
        // Do the following:
        //   - Get current time
        //   - *Check if any processes need to move from NotStarted to Ready (based on elapsed time), and if so put that process in the ready queue
        //   - *Check if any processes have finished their I/O burst, and if so put that process back in the ready queue
        //   - *Check if any running process need to be interrupted (RR time slice expires or newly ready process has higher priority)
        //     - NOTE: ensure processes are inserted into the ready queue at the proper position based on algorithm
        //   - Determine if all processes are in the terminated state
        //   - * = accesses shared data (ready queue), so be sure to use proper synchronization
        uint64_t ct = currentTime();

        
        for(int i = 0; i < processes.size(); i++){
            if (processes[i]->getState() == Process::State::NotStarted && ct >= processes[i]->getStartTime()){

                processes[i]->setState(Process::State::Ready, ct);
                    
                {
                    std::lock_guard<std::mutex> lock(shared_data->queue_mutex);
                    if(shared_data->algorithm == ScheduleAlgorithm::SJF){
                        std::list<Process*>::iterator itr;
                        bool foundOne = false;
                        for(itr = shared_data->ready_queue.begin(); itr != shared_data->ready_queue.end(); itr++){
                            if((**itr).getRemainingTime() > processes[i]->getRemainingTime()){
                                shared_data->ready_queue.insert(itr, processes[i]);
                                foundOne = true;
                                break;
                            }
                        }
                        if(!foundOne){
                            shared_data->ready_queue.push_back(processes[i]);
                        }
                    }
                    else if(shared_data->algorithm == ScheduleAlgorithm::PP){
                        std::list<Process*>::iterator itr;
                        bool foundOne = false;
                        for(itr = shared_data->ready_queue.begin(); itr != shared_data->ready_queue.end(); itr++){
                            if((**itr).getPriority() > processes[i]->getPriority()){
                                shared_data->ready_queue.insert(itr, processes[i]);
                                foundOne = true;
                                break;
                            }
                        }
                        if(!foundOne){
                            shared_data->ready_queue.push_back(processes[i]);
                        }
                    }
                    else{
                        shared_data->ready_queue.push_back(processes[i]);
                    }
                }
            }
        }
        

        for(int i = 0; i < processes.size(); i++){
            if(processes[i]->getState() == Process::State::IO){
                if(ct-processes[i]->getBurstStartTime() > processes[i]->getBurstTimeAtIndex(processes[i]->getBurstIndex())){

                    processes[i]->setState(Process::State::Ready, ct);

                    {
                        std::lock_guard<std::mutex> lock(shared_data->queue_mutex);
                        shared_data->ready_queue.push_back(processes[i]);

                        // if(shared_data->algorithm == ScheduleAlgorithm::SJF){
                        //     std::list<Process*>::iterator itr;
                        //     bool foundOne = false;
                        //     for(itr = shared_data->ready_queue.begin(); itr != shared_data->ready_queue.end(); itr++){
                        //         if((**itr).getRemainingTime() > processes[i]->getRemainingTime()){
                        //             shared_data->ready_queue.insert(itr, processes[i]);
                        //             foundOne = true;
                        //             break;
                        //         }
                        //     }
                        //     if(!foundOne){
                        //     shared_data->ready_queue.push_back(processes[i]);
                        //     }
                        // }
                        // else if(shared_data->algorithm == ScheduleAlgorithm::PP){
                        //     std::list<Process*>::iterator itr;
                        //     bool foundOne = false;
                        //     for(itr = shared_data->ready_queue.begin(); itr != shared_data->ready_queue.end(); itr++){
                        //         if((**itr).getPriority() > processes[i]->getPriority()){
                        //             shared_data->ready_queue.insert(itr, processes[i]);
                        //             foundOne = true;
                        //             break;
                        //         }
                        //     }
                        //     if(!foundOne){
                        //     shared_data->ready_queue.push_back(processes[i]);
                        //     }
                        // }
                        // else{
                        //     shared_data->ready_queue.push_back(processes[i]);
                        // }
                    }
                }
            }
        }
        

        // for(int i = 0; i < processes.size(); i++){
        //     // if(shared_data->algorithm == ScheduleAlgorithm::RR){
        //     //     if(processes[i]->getState() == Process::State::Running){
        //     //         if(processes[i]->burstTimeExpired(ct)){
        //     //             processes[i]->updateBurstTime(processes[i]->getBurstIndex(), processes[i]->getBurstTimeAtIndex(processes[i]->getBurstIndex()) - (ct-processes[i]->getBurstStartTime()));
        //     //             processes[i]->interrupt();
        //     //         }
        //     //     }
        //     // }
        //     if(shared_data->algorithm == ScheduleAlgorithm::PP){
        //         bool shouldPreempt = false;
        //         {
        //             std::lock_guard<std::mutex> lock(shared_data->queue_mutex);
        //             if(!shared_data->ready_queue.empty() && shared_data->ready_queue.front()->getPriority() > processes[i]->getPriority()){
        //                 shouldPreempt = true;
        //             }
        //         }
                
        //         if (shouldPreempt && processes[i]->getState() == Process::State::Running)
        //         {
        //             processes[i]->interrupt();
        //         }
        //     }
        // }
        

        shared_data->all_terminated = true;
        for(int i = 0; i < processes.size(); i++){
            if(processes[i]->getState() != Process::State::Terminated){
                shared_data->all_terminated = false;
                break;
            }
        }
        
        
        // Maybe simply print progress bar for all procs?
        printProcessOutput(processes);

        // sleep 50 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // clear outout
        erase();
    }


    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }

    // print final statistics (use `printw()` for each print, and `refresh()` after all prints)
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time
    uint64_t end = currentTime();

    double utilization = 0;
    for(int i = 0; i < processes.size(); i++){
        utilization += processes[i]->getTotalRunTime();
    }
    utilization /= end-start;
    printw("CPU Utilization: %.2f\n", utilization);

    std::vector<u_int32_t> tempList;
    for(int i = 0; i < processes.size(); i++){
        tempList.push_back(processes[i]->getTurnaroundTime());
    }
    std::sort(tempList.begin(), tempList.end());
    double halfProcDone = tempList[processes.size()/2];
    printw("\nAverage Throughput for first half of processes finished: %.2f\n", processes.size()/(halfProcDone-start));
    printw("\nAverage Throughput for second half of processes finished: %.2f\n", processes.size()/(end-halfProcDone));
    printw("\nCPU Throughput Overall: %lu\n", processes.size()/(end-start));

    double turn = 0;
    for(int i = 0; i < processes.size(); i++){
        turn += processes[i]->getTurnaroundTime();
    }
    printw("\nAverage Turnaround Time: %.2f\n", turn/processes.size());

    double waiting = 0;
    for(int i = 0; i < processes.size(); i++){
        waiting += processes[i]->getWaitTime();
    }
    printw("\nAverage Waiting Time: %.2f\n", waiting/processes.size());
    refresh();

    // Clean up before quitting program
    processes.clear();
    endwin();

    return 0;
}

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
    // Work to be done by each core idependent of the other cores
    // Repeat until all processes in terminated state:
    while (!shared_data->all_terminated)
    {
    
    //   - *Get process at front of ready queue
        Process* proc = nullptr;
        {
            std::lock_guard<std::mutex> lock(shared_data->queue_mutex);
            if (!shared_data->ready_queue.empty())
            {
                proc = shared_data->ready_queue.front();
                shared_data->ready_queue.pop_front();
            }
        }

    //   - IF READY QUEUE WAS NOT EMPTY
        if (proc != nullptr)
        {
    //    - Wait context switching load time
            std::this_thread::sleep_for(std::chrono::milliseconds(shared_data->context_switch));

    //    - Simulate the processes running (i.e. sleep for short bits, e.g. 5 ms, and call the processes `updateProcess()` method)
    //      until one of the following:
    //      - CPU burst time has elapsed
    //      - Interrupted (RR time slice has elapsed or process preempted by higher priority process)

            uint64_t start = currentTime();
            proc->setCpuCore(core_id);
            proc->setState(Process::State::Running, start);
            proc->setBurstStartTime(start);
            proc->interruptHandled();

            int startburst = proc->getCurrentBurst();
            uint64_t startSlice = start;

            while(true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                uint64_t now = currentTime();
                proc->updateProcess(now);
                //   - Place the process back in the appropriate queue

                // If CPU burst time has elapsed
                    //      - I/O queue if CPU burst finished (and process not finished) -- no actual queue, simply set state to IO
                    //      - Terminated if CPU burst finished and no more bursts remain -- set state to Terminated
                if (proc->getCurrentBurst() != startburst)
                {
                    proc->setCpuCore(-1);

                    // If no more bursts, terminate
                    if (proc->getCurrentBurst() >= proc->getNumBursts())
                    {
                        proc->setState(Process::State::Terminated, now);
                    }

                    else
                    {
                        proc->setState(Process::State::IO, now);
                        proc->setBurstStartTime(now);
                    }

                    break;
                }

                // If RR time slice has elapsed
                if (shared_data->algorithm == ScheduleAlgorithm::RR)
                {
                    if (now - startSlice >= shared_data->time_slice)
                    {
                        proc->interrupt();
                    }
                }

                // If process preempted by higher priority process
                    //      - *Ready queue if interrupted (be sure to modify the CPU burst time to now reflect the remaining time)
                if (shared_data->algorithm == ScheduleAlgorithm::PP)
                {
                    Process* highest_ready = nullptr;

                    {
                        std::lock_guard<std::mutex> lock(shared_data->queue_mutex);

                        if (!shared_data->ready_queue.empty())
                        {
                            highest_ready = shared_data->ready_queue.front(); 
                            // NOTE: assumes queue is already sorted by priority
                        }
                    }

                    // If there is a higher-priority process than the current running one
                    if (highest_ready != nullptr &&
                        highest_ready->getPriority() < proc->getPriority())
                    {
                        proc->interrupt();
                    }
                }
                
                if (proc->isInterrupted())
                {
                    proc->setCpuCore(-1);
                    proc->setState(Process::State::Ready, now);
                    uint64_t elapsed = now - proc->getBurstStartTime();
                    proc->updateBurstTime(proc->getBurstIndex(), proc->getBurstTimeAtIndex(proc->getBurstIndex()) - elapsed);
                    proc->setBurstStartTime(now);
                    {
                        std::lock_guard<std::mutex> lock(shared_data->queue_mutex);
                        shared_data->ready_queue.push_back(proc);
                    }

                    proc->interruptHandled();
                    break;
                }
            }

    //   - Wait context switching save time
            std::this_thread::sleep_for(std::chrono::milliseconds(shared_data->context_switch));
        }

    //  - IF READY QUEUE WAS EMPTY
        else
        {
            //   - Wait short bit (i.e. sleep 5 ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            //  - * = accesses shared data (ready queue), so be sure to use proper synchronization
        }
    }
}

void printProcessOutput(std::vector<Process*>& processes)
{
    printw("|   PID | Priority |    State    | Core |               Progress               |\n"); // 36 chars for prog
    printw("+-------+----------+-------------+------+--------------------------------------+\n");
    for (int i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double total_time = processes[i]->getTotalRunTime();
            double completed_time = total_time - processes[i]->getRemainingTime();
            std::string progress = makeProgressString(completed_time / total_time, 36);
            printw("| %5u | %8u | %11s | %4s | %36s |\n", pid, priority,
                   process_state.c_str(), cpu_core.c_str(), progress.c_str());
        }
    }
    refresh();
}

std::string makeProgressString(double percent, uint32_t width)
{
    uint32_t n_chars = percent * width;
    std::string progress_bar(n_chars, '#');
    progress_bar.resize(width, ' ');
    return progress_bar;
}

uint64_t currentTime()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
