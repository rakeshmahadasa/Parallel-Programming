#include <iostream>
#include <pthread.h>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <queue>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

#ifndef MAX_DIST
#define MAX_DIST 10000000
#endif

typedef long long int lli;
typedef unsigned long long int ulli;
typedef unsigned int ui;
typedef pair<int, int> pii;
typedef pair<lli, lli> pll;
typedef vector<int> vi;
typedef vector<long long int> vli;
typedef vector<vi> vvi;
typedef vector<vli> vvl;
int NUM_THREADS = 1;

int array_size = 0;
int input[100000001];


void generate_random_input(){
    for(int i = 0; i < array_size;i++){
        input[i] = rand()%100 + 1;
    }
}

void merge(int s1,int e1,int s2,int e2){
    int buffer[e2-s1+1];
    int idx=0;
    int ls=s1,rs=s2;
    while(ls<=e1 && rs<=e2){
        if(input[ls]<=input[rs]){
            buffer[idx]=input[ls];
            ls++;
        }
        else{
            buffer[idx]=input[rs];
            rs++;
        }
        idx++;
    }
    for(int i = ls;i<=e1;i++){
        buffer[idx]=input[i];
        idx++;
    }
    for(int i = rs;i<=e2;i++){
        buffer[idx]=input[i];
        idx++;
    }
    for(int i = 0; i < idx;i++){
        input[s1+i] = buffer[i];
    }
}

int mergesort(int start,int end){
    if(start>=end){
        return 0;
    }
    else{
        int mid = start + (end-start)/2;
        mergesort(start,mid);
        mergesort(mid+1,end);
        merge(start,mid,mid + 1,end);
    }
}

struct args{
    int start;
    int end;
};

void *parallel_msort(void *index_args){
    struct args *curr_index_args = (struct args *)index_args;  
    int start = curr_index_args->start;
    int end = curr_index_args->end;
        if(start>=end){
        pthread_exit(0);
    }
    else{
        int curr_array_size = (end - start + 1);
        if (curr_array_size > 10000)
        {
            int mid = start + (end - start) / 2;
            pthread_t left, right;
            struct args largs, rargs;
            largs.start = start;
            largs.end = mid;
            rargs.start = mid + 1;
            rargs.end = end;
            pthread_create(&left, NULL, parallel_msort, &largs);
            pthread_create(&right, NULL, parallel_msort, &rargs);
            pthread_join(left, NULL);
            pthread_join(right, NULL);
            merge(start, mid, mid + 1, end);
            pthread_exit(0);
        }
        else{
            int mid = start + (end-start)/2;
            mergesort(start,mid);
            mergesort(mid+1,end);
            merge(start,mid,mid + 1,end);
        }
    }
}

void print_array(){
    for(int i = 0; i < array_size;i++){
        cout<<input[i]<<" ";
    }
    cout<<endl;
}
int main(int argc, char const *argv[])
{
    if(argc<2){
        cerr<<"Minimum one argument required."<<endl;
        exit(-1);
    }
    array_size = stoi(argv[1]);
    generate_random_input();

    pthread_t main_thread;
    struct args index_args;
    index_args.start=0;
    index_args.end=array_size-1;
    //print_array();
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    pthread_create(&main_thread,NULL,parallel_msort,&index_args);
    pthread_join(main_thread,NULL);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout<<duration_cast<microseconds>(tend - tstart).count() <<endl;
    //print_array();
    return 0;
}
