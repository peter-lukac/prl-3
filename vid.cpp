/**
 * PRL project 3: visibility problem
 * 
 * @filename: vid.cpp
 * @author: Peter Lukac
 * @login: xlukac11
 * 
 * April 2020
 * */

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <math.h>
#include <stdlib.h>

#define TAG 0

/**
 * Parses input values in string and returns vector of ints
 * @param str string of int values separated by comma
 * @return vector of integers
 * */
std::vector<int> parse_values(const std::string &str, const int start = 0){
    std::vector<int> vec = std::vector<int>();

    size_t comma_idx;
    size_t last_pos = 0;
    while( (comma_idx = str.find(',', last_pos)) != std::string::npos ){
        if ( comma_idx == str.length()-1 || comma_idx == last_pos )
            std::cerr << "wrong input format" << std::endl;
        vec.push_back(std::stoi(str.substr(last_pos, comma_idx-last_pos)));
        last_pos = comma_idx+1;
    }
    vec.push_back(std::stoi(str.substr(last_pos, str.length()-last_pos)));

    return vec;
}

/**
 * Prescan function implemented with max operation
 * @param proc_idx index of the process
 * @param proc_count number of the processes in the proces tree
 * @param max_angle process value
 * @return max prescaned value
 * */
float max_perscan(const int proc_idx, const int proc_count, float max_angle){
    MPI_Status mpi_status; 
    // get number of iterations
    int itter = ceil(log2(proc_count));
    // Up-Sweep
    for ( int j = 0; j < itter; j++){
        // if process is in the tree ...
        if ( (proc_idx+1)%(int)pow(2,j) == 0 ){
            // process is reciving if it has left neighbour
            if ( (proc_idx+1)%(int)pow(2,j+1) == 0 ){
                float new_angle;
                MPI_Recv(&new_angle, sizeof(max_angle), MPI_FLOAT, proc_idx - pow(2,j), TAG, MPI_COMM_WORLD, &mpi_status);
                max_angle = std::max(new_angle, max_angle);
            }
            // else process is sending to right neighbour
            else{
                MPI_Send(&max_angle, sizeof(max_angle), MPI_FLOAT, proc_idx + pow(2,j), TAG, MPI_COMM_WORLD);
            }
        }

    }
    // Down-Sweep    
    if ( proc_count - proc_idx  == 1 )
        max_angle = -2; // last process will have minimum value that will be distributed to the left
    for ( int j = itter-1; j >= 0 ; j--){
        // if process is in the tree ...
        if ( (proc_idx+1)%(int)pow(2,j) == 0 ){
            // process is sending if it has left neighbour, then gets back its value and keeps maximum
            if ( (proc_idx+1)%(int)pow(2,j+1) == 0 ){
                MPI_Send(&max_angle, sizeof(max_angle), MPI_FLOAT, proc_idx - pow(2,j), TAG, MPI_COMM_WORLD);
                float new_angle;
                MPI_Recv(&new_angle, sizeof(max_angle), MPI_FLOAT, proc_idx - pow(2,j), TAG, MPI_COMM_WORLD, &mpi_status);
                max_angle = std::max(max_angle, new_angle);
            }
            // else process is reciving from the right neighbour and sends back own value
            else{
                float new_angle;
                MPI_Recv(&new_angle, sizeof(max_angle), MPI_FLOAT, proc_idx + pow(2,j), TAG, MPI_COMM_WORLD, &mpi_status);
                MPI_Send(&max_angle, sizeof(max_angle), MPI_FLOAT, proc_idx + pow(2,j), TAG, MPI_COMM_WORLD);
                max_angle = new_angle;
            }
        }
    }
    return max_angle;
}

/**
 * Main
 * @param argc
 * @param argv input string in argv[1]
 * */
int main(int argc, char** argv){

    MPI_Status mpi_status; 
    int proc_idx;
    int proc_count;

    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_idx);
    
    if ( argc == 1 ){
        MPI_Finalize();
        return 1;
    }

    // vectors for all 4 kinds of data
    std::vector<int> alt = parse_values(std::string(argv[1]));
    std::vector<float> angle = std::vector<float>(alt.size(), 0.0);
    std::vector<float> max_prev_angle = std::vector<float>(alt.size(), 0.0);
    std::vector<char> visible = std::vector<char>(alt.size(), ' ');

    // splitting input into chunks
    int chunk_size = alt.size()/proc_count;
    if ( alt.size()%proc_count > 0 && proc_idx < alt.size()%proc_count )
        chunk_size++;
    int chunk_start = (alt.size()/proc_count)*proc_idx + std::min(proc_idx, (int)(alt.size()%proc_count));
    if ( proc_idx == 0 ){
        chunk_start++;
        chunk_size--;
        visible[0] = '_';
    }
    max_prev_angle[chunk_start-1] = -2;

    //auto start_time = std::chrono::steady_clock::now();

    // calculate angle and local maximum previous angle
    for ( int i = chunk_start; i < chunk_start + chunk_size; i++){
        angle[i] = atan((float)(alt[i] - alt[0])/(float)i);
        if ( angle[i] > max_prev_angle[i-1] )
            max_prev_angle[i] = angle[i];
        else
            max_prev_angle[i] = max_prev_angle[i-1];
    }

    // get prescaned maxium angle for each chunk
    float max_angle = max_perscan(proc_idx, proc_count, max_prev_angle[chunk_start + chunk_size-1]);

    // calculate visibility, if angle is bigger than prescaned maximum angle, then is visible
    for ( int i = chunk_start; i < chunk_start + chunk_size; i++){
        if ( angle[i] >  max_prev_angle[i-1] &&  angle[i] > max_angle )
            visible[i] = 'v';
        else
            visible[i] = 'u';
    }

    //auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start_time).count();
    
    // collect data from all processes
    if ( proc_idx == 0 ){
        // main process will print its own visibility and then collect visibility from others
        std::cout << "_";
        for ( int i = chunk_start; i < chunk_start + chunk_size; i++){
            std::cout << "," << visible[i];
        }
        //std::cerr << time << " "; // print time
        char v;
        for ( int i = 1; i < proc_count; i++ ){
            MPI_Recv(&v, 1, MPI_UNSIGNED_CHAR, i, TAG, MPI_COMM_WORLD, &mpi_status);
            while(v != 'x'){
                std::cout << "," << v;
                MPI_Recv(&v, 1, MPI_UNSIGNED_CHAR, i, TAG, MPI_COMM_WORLD, &mpi_status);
            }
        }
        std::cout << std::endl;
    } else {
        // other processes will send visibility to main process
        for ( int i = chunk_start; i < chunk_start + chunk_size; i++){
            MPI_Send(&visible[i], 1, MPI_UNSIGNED_CHAR, 0, TAG, MPI_COMM_WORLD);
        }
        char end = 'x';
        MPI_Send(&end, 1, MPI_UNSIGNED_CHAR, 0, TAG, MPI_COMM_WORLD);
        //std::cerr << time << " "; // print time
    }
    
    MPI_Finalize();
    return 0;
}