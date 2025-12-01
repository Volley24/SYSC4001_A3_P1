/**
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @brief template main.cpp file for Assignment 3 Part 1 of SYSC4001
 * 
 */

#include <interrupts_student1_student2.hpp>

using namespace std;

// not needed
// void FCFS(std::vector<PCB> &ready_queue) {
//     std::sort( 
//                 ready_queue.begin(),
//                 ready_queue.end(),
//                 []( const PCB &first, const PCB &second ){
//                     return (first.arrival_time > second.arrival_time); 
//                 } 
//             );
// }

// void EP(std::vector<PCB> &ready_queue) {
//     std::sort( 
//                 ready_queue.begin(),
//                 ready_queue.end(),
//                 []( const PCB &first, const PCB &second ){
//                     return (first.PID > second.PID); 
//                 } 
//             );
// }

/*
 Algorithm to implement: External Priorities without preemption.

*/

std::tuple<std::string /* add std::string for bonus mark */ > run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   //The ready queue of processes
    std::vector<PCB> wait_queue;    //The wait queue of processes
    std::vector<PCB> job_list;      //A list to keep track of all the processes. This is similar
                                    //to the "Process, Arrival time, Burst time" table that you
                                    //see in questions. You don't need to use it, I put it here
                                    //to make the code easier :).

    unsigned int current_time = 0;
    PCB running;

    //Initialize an empty running process
    idle_CPU(running);

    std::string execution_status;

    //make the output table (the header row)
    execution_status = print_exec_header();

    unsigned int r = 5;
    unsigned int r_remaining = r;

    // so we don't run into an infinite loop.
    // this is temporary
    int max_iters = 150;

    //Loop while till there are no ready or waiting processes.
    //This is the main reason I have job_list, you don't have to use it.
    while((!all_process_terminated(job_list) || job_list.empty()) && max_iters > 0) {
        //Inside this loop, there are three things you must do:
        // 1) Populate the ready queue with processes as they arrive
        // 2) Manage the wait queue
        // 3) Schedule processes from the ready queue

        //Population of ready queue is given to you as an example.
        //Go through the list of proceeses
        for(auto &process : list_processes) {

            if(process.state == NOT_ASSIGNED && current_time >= process.arrival_time) {
                //if so, assign memory and put the process into the ready queue
                bool success = assign_memory(process);

                if (!success) {
                    continue; // ERROR: No memory for new process! Process must stay in NEW state.
                }

                process.state = READY;  //Set the process state to READY
                ready_queue.push_back(process); //Add the process to the ready queue
                job_list.push_back(process); //Add it to the list of processes

                execution_status += print_exec_status(current_time, process.PID, NEW, READY);
            }
        }

        ///////////////////////MANAGE WAIT QUEUE/////////////////////////
        //This mainly involves keeping track of how long a process must remain in the ready queue

        // WAITING -> READY
        for (auto &process : wait_queue) {
            // NEW FROM EP: cpu_remamining_before_io might not be 0 while a process is in the WAITING queue,
            // Because it might have been kicked out before it's time to do I/O
            // i,e: Process A runs I/O every 5ms, but RR kicked it out at 4ms.
            // process A still needs to wait, but CANNOT DO I/O
            // next time it runs, after 1ms, it will go back into waiting queue and do I/O.
            if (process.state == WAITING) {
                process.io_remaining --;

                if (process.io_remaining == 0) {
                    // Put the waiting process back into the ready queue, as it finished I/O.
                    process.state = READY;  
                    ready_queue.push_back(process); 
                    sync_queue(job_list, process);

                    execution_status += print_exec_status(current_time, process.PID, WAITING, READY);
                }
            }
        }

        bool should_run_new_process = false;

        if (running.state == RUNNING) {

            r_remaining --; // rr time remaining left
            running.remaining_time --; // global remaining time left 
            running.cpu_remamining_before_io --; // not used if io_duration == 0

            if (running.remaining_time == 0) {
                // RUNNING -> TERMINATED
                terminate_process(running, job_list);
                execution_status += print_exec_status(current_time, running.PID, RUNNING, TERMINATED);

                should_run_new_process = true; // a process terminated, so we need to run one
            }else if (running.io_duration != 0 && running.cpu_remamining_before_io == 0) {
                // RUNNING -> WAITING
                running.state = WAITING;
                running.io_remaining = running.io_duration; // update corresponding i/o remaining with i/o duration
                running.cpu_remamining_before_io = running.io_freq;

                wait_queue.push_back(running);
                sync_queue(job_list, running);

                cout << "P" << running.PID << ": is now waiting for I/O to complete. i/o remaining = " << running.io_duration << endl;

                // RUNNING -> WAITING
                execution_status += print_exec_status(current_time, running.PID, RUNNING, WAITING);

                should_run_new_process = true; // a process is waiting, so we need to run one
            } else if (r_remaining == 0) {
                // RUNNING -> READY.

                std::cout << "P" << running.PID << " was kicked out from RR timer." << std::endl;

                running.state = READY;

                // If the current ran process doesn't have any I/O
                // Push it to the back of the queue instantly.
                // Else, we'll have to push it back after it's done WAITING for I/O to complete.
               
                std::cout << "P" << running.PID << " doesn't have any I/O, push back to queue instantly" << endl;
                ready_queue.push_back(running); 

                sync_queue(job_list, running);

                // RUNNING -> READY
                execution_status += print_exec_status(current_time, running.PID, RUNNING, READY);

                // No waiting needed here.

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
            
            r_remaining = r; // reset RR counter.
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(current_time, running.PID, READY, RUNNING);
        }        
        /////////////////////////////////////////////////////////////////

        current_time ++;
        max_iters --;
    }
    
    //Close the output table
    execution_status += print_exec_footer();

    std::cout << "Using RR with R = " << r << "ms" << std::endl;

    return std::make_tuple(execution_status);
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

    if (argc == 3) {
        output_file_name = argv[2];
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
    auto [exec] = run_simulation(list_process);

    // Write to the OUTPUT file
    write_output(exec, output_file_name);

    return 0;
}