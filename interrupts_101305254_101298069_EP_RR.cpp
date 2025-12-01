/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 * @author Maxim Creanga
 */

#include <interrupts_101305254_101298069.hpp>

using namespace std;

/*
 Algorithm to implement: Round-Robin with external priorites (and preemption)

 This works the same as RR, with two crucial details:
 1) The ready-queue needs to be sorted, like in EP, according to priorities. 
    We don't even need multiple queues to handle the possibilites of multiples RR's for each given priority,
    Because assuming we use a "stable" sorting algorithm, each individual priorities' RR queues remain unchanged.

    For example:

    Consider the queue:
    (rr for PID = 1)[A (1), B (1), C (1)], (rr for PID = 0, only has D for now.)[D (0)] << D was JUST added.

    When the queue is SORTED (the EP_RR function) before a process is ran it will look like this:
    (rr for PID = 0, only has D for now.)[D (0)], (rr for PID = 1)[A (1), B (1), C (1)].

    D was succesfully moved to the front, without "disturbing" the exisiting RR queue for PID = 1.
    As such we don't need a seperate ready_queue for each priority.

 2) However, as shown above, if a new process becomes ready (i.e: D goes from WAITING -> READY) while a lower priority process is running (in RR fashion)
    Said process must instantly be preempted, and moved "to the back of the line" for the given RR queue.
    Since the sorting handles everything, it's just pushed back to the end of ready_queue, and when it is sorted, each of the individual RR queues for each priority will be visible.
    (As such we don't need a ready_queue for each priority)
*/

// Same as EP, but stable sort is NEEDED. See comment above for explanation.
void EP_RR(std::vector<PCB> &ready_queue) {
    std::stable_sort( 
                ready_queue.begin(),
                ready_queue.end(),
                []( const PCB &first, const PCB &second ){
                    // Note: This is NOT inverted. In the .hpp file, for ease-of-usage
                    // ready_queue.back() was changed to ready_queue.front()
                    return (first.PID < second.PID); 
                } 
            );
}

std::tuple<std::string, std::string> run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    std::vector<int> temp_processes_indices_exiting_waiting_queue;   // Temporary queue to keep track of processes (their indices) leaving the waiting queue

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;
    std::string memory_output;

    //make the output table (the header row)
    execution_status = print_exec_header();

    unsigned int r = 100; 
    unsigned int r_remaining = r;


    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while((!all_process_terminated(job_list) || job_list.empty()) && current_time < 1000) {
        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        bool higher_priority_interrupt = false;

        // The template for the population of the ready was slightly modified.
        // i) A process may not be able to be assigned memory, as partitions have been allocated in such a way that said process cannot be allocated.
        // ii) As a result of this, a process may be put into the READY queue AFTER it's intended arrival time. The IF statement was changed to allow this.
        for(auto &process : list_processes) {

            if(process.state == NOT_ASSIGNED && current_time >= process.arrival_time) {
                //if so, assign memory and put the process into the ready queue
                bool success = assign_memory(process);

                if (!success) {
                    continue; // ERROR: No memory for new process! Process must stay in NEW state.
                }

                process.state = READY;  // Set the process state to READY
                // record time when the process entered READY
                process.ready_enter_time = current_time;
                ready_queue.push_back(process); // Add the process to the ready queue
                job_list.push_back(process); // Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);

                // Append memory snapshot on allocation
                memory_output += record_memory_usage(job_list);

                // [NEW FOR EP_RR]: It's possible here that a newly added process has a higher priority then the currently running one.
                // In that case, the currently running process should immediately STOP, pushed to the back of the ready_queue
                if (running.state == RUNNING && process.PID < running.PID) {
                    higher_priority_interrupt = true;
                }
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue

        // WAITING -> READY process
        // This process involves decrementing I/O remaining ticks for each process in the WAITING queue.
        // Whenever said process is finished I/O, it is re-added to the READY queue.
        for (int i = 0; i < wait_queue.size(); i++) {
            auto &process = wait_queue[i];
            if (process.state == WAITING) {
                process.io_remaining --;

                if (process.io_remaining == 0) {
                    // Put the waiting process back into the ready queue, as it has finished I/O.
                    process.state = READY;  

                    // record when entering READY after I/O
                    process.ready_enter_time = current_time;
                    ready_queue.push_back(process); 
                    sync_queue(job_list, process);

                    temp_processes_indices_exiting_waiting_queue.push_back(i);
                    execution_status += print_exec_status(current_time, process.PID, WAITING, READY);

                    // [NEW FOR EP_RR]: It's possible here that the newly ready process has a higher priority then the currently running one.
                    // In that case, the currently running process should immediately STOP, pushed to the back of the ready_queue
                    if (running.state == RUNNING && process.PID < running.PID) {
                        higher_priority_interrupt = true;
                    }
                }
            }
        }

        for (int i = 0; i < temp_processes_indices_exiting_waiting_queue.size(); i ++) {
            // We need this so that indices don't get broken when we start erasing.
            int index_backwards = temp_processes_indices_exiting_waiting_queue.size() - 1 - i;

            // Find index to erase based on index of index. (a bit confusing, but this is actually grabbing the index we need to erase from the wait_queue)
            int index_to_erase = temp_processes_indices_exiting_waiting_queue[index_backwards];

            wait_queue.erase(wait_queue.begin() + index_to_erase);
        }

        bool should_run_new_process = false;

        if (running.state == RUNNING) {
 
            r_remaining --; // RR time remaining left [NEW FOR RR!]
            running.remaining_time --; // Global remaining time left 
            running.cpu_remamining_before_io --; // Not used if io_duration == 0

            if (running.remaining_time == 0) {
                // RUNNING -> TERMINATED
                // If a process doesn't have any remaining time left, TERMINATE it.

                // set finish time for turnaround calculation
                running.finish_time = current_time;
                terminate_process(running, job_list, current_time);
                execution_status += print_exec_status(current_time, running.PID, RUNNING, TERMINATED);

                should_run_new_process = true; // a process terminated, so we need to run one
            }else if (running.io_duration != 0 && running.cpu_remamining_before_io == 0) {
                // RUNNING -> WAITING
                // If a process has reached a point where it needs to execute I/O, move it to the WAIT queue.

                // Find the I/O intervals
                int interval = 0;
                if (running.last_io_time != -1) {
                    interval = current_time - running.last_io_time;
                } else {
                    interval = current_time - running.start_time;
                }
                running.io_interval_sum += interval;
                running.io_interval_count += 1;
                running.last_io_time = current_time;

                running.state = WAITING;
                running.io_remaining = running.io_duration; // update corresponding i/o remaining with i/o duration
                running.cpu_remamining_before_io = running.io_freq;

                wait_queue.push_back(running);
                sync_queue(job_list, running);

                // RUNNING -> WAITING
                execution_status += print_exec_status(current_time, running.PID, RUNNING, WAITING);

                should_run_new_process = true; // a process is waiting, so we need to run one
            } else if (r_remaining == 0 || higher_priority_interrupt) {
                // [NEW FOR EP_RR]: The higher_priority_interrupt works in the exact same way as the RR interrupt.
                // If higher_priority_interrupt and I/O needs to be done, WAITING takes priority over READY.

                // [NEW FOR RR!]
                // RUNNING -> READY
                // If the RR time quantum has been reached, the process running must be kicked out. 
                // It will be moved to ready, as it had been interrupted in the middle of an I/O sequence.
                // As the RUNNING -> WAITING queue ELSE-IF is above this one, if a process runs out of R time AND needs to do I/O at the same time,
                // Obviously the I/O takes priority.

                running.state = READY;
                ready_queue.push_back(running); 
                sync_queue(job_list, running);

                // RUNNING -> READY
                execution_status += print_exec_status(current_time, running.PID, RUNNING, READY);


                should_run_new_process = true; // a process is waiting, so we need to run one
            }
        }else {
            // Initiallly, no processes are running, so we need to run one 
            should_run_new_process = true;
        }


        /////////////////////////////////////////////////////////////////

        //////////////////////////SCHEDULER//////////////////////////////

        // READY -> RUNNING
        if (should_run_new_process && !ready_queue.empty()) {
            // See comment at the start of the file to see why this works, and why we only need one ready_queue
            EP_RR(ready_queue);
            
            r_remaining = r; // Reset the RR counter for the next process to run
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(current_time, running.PID, READY, RUNNING);
        }        
        /////////////////////////////////////////////////////////////////

        current_time ++;
        temp_processes_indices_exiting_waiting_queue.clear();
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    std::cout << "Using RR with R = " << r << "ms" << std::endl;

    // Record memory usage snapshot (written to memory_output file)
    memory_output += record_memory_usage(job_list);

    // Compute metrics: Throughput, Avg Wait Time, Avg Turnaround Time, Avg Response Time 
    int total_wait = 0;
    int total_turnaround = 0;
    int completed_jobs = 0;
    int max_finish_time = -1;
    int total_first_response = 0;
    int first_response_count = 0;

    // Sum up all the waiting times
    for (auto &process : job_list) {
        total_wait += process.total_wait_time;

        if (process.finish_time != -1) {
            total_turnaround += (process.finish_time - process.arrival_time);
            completed_jobs += 1;
            if (process.finish_time > max_finish_time) 
				max_finish_time = process.finish_time;
        }

        if (process.first_response_time != -1) {
            total_first_response += (process.first_response_time - process.arrival_time);
            first_response_count += 1;
        }
    }

    int simulation_end_time = (max_finish_time != -1) ? max_finish_time : current_time;

    // Math to figure out averages!
    double throughput_proc_per_tick = 0.0;
    if (simulation_end_time > 0 && completed_jobs > 0) throughput_proc_per_tick = (double) completed_jobs / (double) simulation_end_time;
    double avg_wait = (completed_jobs > 0) ? (double) total_wait / (double) completed_jobs : 0.0;
    double avg_turnaround = (completed_jobs > 0) ? (double) total_turnaround / (double) completed_jobs : 0.0;
    double avg_response_time = (first_response_count > 0) ? (double) total_first_response / (double) first_response_count : 0.0;

    stringstream metrics; // buffer so we can << all of our metrics
    metrics << "\n=== Simulation Metrics ===\n";
    metrics << "Throughput (processes per ms): " << throughput_proc_per_tick << "\n";
    metrics << "Average Turnaround Time: " << avg_turnaround << " ms\n";
    metrics << "Average Wait Time: " << avg_wait << " ms\n";
    metrics << "Average Response Time (arrival -> first run): " << avg_response_time << " ms\n";

    execution_status += metrics.str();

    return std::make_tuple(execution_status, memory_output);
}



int main(int argc, char** argv) {
    // Note: This was modified to be able to spesify an output file too.
    // This make the generation / verification much easier.

    //Get the input file from the user
    if(argc < 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrutps <your_input_file.txt>" << std::endl;
        return -1;
    }

    // Default output file name.
    // So that there are no breaking changes.
    auto output_file_name = "execution.txt";
    auto mem_file_name = "memory_output.txt";

    if (argc >= 3) {
        output_file_name = argv[2];
    }
    if (argc >= 4) {
        mem_file_name = argv[3];
    }

    //Open the input file
    auto file_name = argv[1];
    std::ifstream input_file;
    input_file.open(file_name);

    //Ensure that the file actually opens
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    //Parse the entire input file and populate a vector of PCBs.
    //To do so, the add_process() helper function is used (see include file).
    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    //With the list of processes, run the simulation
    auto [exec, mem] = run_simulation(list_process);

    // Write to the OUTPUT files
    write_output(exec, output_file_name);
    write_output(mem, mem_file_name);

    return 0;
}