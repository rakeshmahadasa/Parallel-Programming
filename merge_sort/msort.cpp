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
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    mergesort(0,array_size-1);
    high_resolution_clock::time_point tend = high_resolution_clock::now();

    cout<<duration_cast<microseconds>(tend - tstart).count() <<endl;
    return 0;
}
