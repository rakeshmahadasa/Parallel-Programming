#include <iostream>
#include<pthread.h>
#include<vector>
#include <stdlib.h>
#include<time.h>
#include<chrono>

using namespace std;
using namespace std::chrono;


#ifndef MAX_DIST
#define MAX_DIST 10000000
#endif

long long int vertexCount=0;
unsigned long long int edgeCount=0;
vector<vector<pair<int,int>>> graph(10000001);
vector<pair<int,int>> dist;
vector<int> indexMap;
bool check_duplicate_edge(int e1,int e2){
    
    for(int i = 0; i < graph[e1].size();i++){
        if(graph[e1][i].first == e2) return true;
    }
    return false;

}

long long int validate_graph_input(){

    if(vertexCount<11){
        cerr<<"Vertex Count Must be greater than 10"<<endl;
        exit(0);
    }
    if(vertexCount>10000000){
        cerr<<"Vertex Count Must be less than 20001"<<endl;
        exit(0);
    }
    if(edgeCount == -1){
        edgeCount=5*vertexCount-1;
    }
    if(edgeCount<(vertexCount-1)){
        cerr<<"Edge Count too less. Atleast "<<vertexCount-1<<" edge needed to build a connected graph with "<<vertexCount<<" vertices"<<endl;
        exit(0);
    }
    unsigned long long int max_edge_count = (vertexCount*(vertexCount-1)/2);
    if(edgeCount>max_edge_count){
        cerr<<"Edge Count too high. At max "<<max_edge_count<<" edges needed to build a fully connected graph with "<<vertexCount<<" vertices"<<endl;
        exit(0);
    }
    return edgeCount;

}
void generate_random_graph(){
    
    for(int i=1;i<=(vertexCount-1);i++){
        int randomWeight = rand()%20+1;
        graph[i].push_back(make_pair(i+1,randomWeight));
        graph[i+1].push_back(make_pair(i,randomWeight));
    }
    int randomEdgeCount=0;
    while(randomEdgeCount<(edgeCount - (vertexCount - 1))){
        int e1 = rand()%vertexCount + 1;
        int e2 = rand()%vertexCount + 1;
        int randomWeight = rand()%20+1;
        if(e1 != e2){
            if(!check_duplicate_edge(e1,e2)){
                graph[e1].push_back(make_pair(e2,randomWeight));
                graph[e2].push_back(make_pair(e1,randomWeight));
                randomEdgeCount++;
            }
        }
    }
    cout<<"Random graph generated "<<"vertex count : "<<vertexCount<<" Edge count : "<<edgeCount<<endl;

}
void heapify(int index,int heapSize){
    int left = 2*index;
    int right = 2*index+1;
    int smallest = dist[index].second;
    int small_pos = index;
    if(left<=heapSize && dist[left].second<smallest){
        smallest=dist[left].second;
        small_pos = left;
    }
    if(right<=heapSize && dist[right].second<smallest){
        smallest=dist[right].second;
        small_pos = right;
    }
    if(smallest == dist[index].second){
        return;
    }
    else {
        indexMap[dist[index].first]=small_pos;
        indexMap[dist[small_pos].first]=index;
        pair<int,int> temp = dist[index];
        dist[index] = dist[small_pos];
        dist[small_pos]=temp;
        heapify(small_pos,heapSize);
    }
}
void build_heap(int heapSize){
    for(int i = heapSize/2;i>=1;i--){
        heapify(i,heapSize);
    }
}
void print_graph(){
    for(int i=1;i<=vertexCount;i++){
        cout<<i<<" ";
        for(int j=0;j<graph[i].size();j++){
            cout<<graph[i][j].first<<" ";
        }
        cout<<endl;
    }
}
void print_graph_with_weights(){
    for(int i=1;i<=vertexCount;i++){
        cout<<i<<" ";
        for(int j=0;j<graph[i].size();j++){
            cout<<"("<<graph[i][j].first<<","<<graph[i][j].second<<")"<<" ";
        }
        cout<<endl;
    }
}
void print_verification_input(int source){
    cout<<vertexCount<<" "<<source-1<<endl;
    for(int i=1;i<=vertexCount;i++){
        cout<<graph[i].size()<<" ";
        for(int j=0;j<graph[i].size();j++){
            cout<<graph[i][j].first-1<<" "<<graph[i][j].second<<" ";
        }
        cout<<endl;
    }
}

void sssp(int source){
    cout<<"Setting distance array"<<endl;
    dist.push_back(make_pair(0,MAX_DIST));
    indexMap.push_back(0);
    for(int i = 1; i <= vertexCount;i++){
        dist.push_back(make_pair(i,MAX_DIST));
        if(i == source) dist[i].second=0;
        indexMap.push_back(i);
    }
    cout<<"Building Heap"<<endl;
    build_heap(vertexCount);
    int heapSize=vertexCount;
    cout<<"Running Algorithm"<<endl;
    while(heapSize){
        pair<int,int> closeVertex = dist[1];
        indexMap[closeVertex.first]=heapSize;
        indexMap[dist[heapSize].first]=1;
        pair<int,int> temp = dist[1];
        dist[1] = dist[heapSize];
        dist[heapSize]=temp;

        for(int i = 0; i < graph[closeVertex.first].size();i++){
            int dist_su = dist[indexMap[closeVertex.first]].second;
            int dist_uv = graph[closeVertex.first][i].second;
            int curr_vertex = graph[closeVertex.first][i].first;
            int dist_sv = dist[indexMap[curr_vertex]].second;
            if( dist_sv > dist_su + dist_uv ){
                dist[indexMap[curr_vertex]].second=dist_su + dist_uv;
            }
        }
        heapSize--;
        build_heap(heapSize);
    }

    for(int i = 1; i <=vertexCount; i++){
        cout<<dist[indexMap[i]].first<<" "<<dist[indexMap[i]].second<<endl;
    }

}
int main(int argc,char**argv){
    //srand ( time(NULL) );
    if(argc<2){
        cerr<<"Required atleast one argument ( vertexCount ). None provided"<<endl;
        cerr<<"Command : ./sssp <vertexCount> <edgeCount-Optional>"<<endl;
        exit(0);
    }
	vertexCount=stoi(argv[1]);
    edgeCount=-1;
    if(argc>2)
	    edgeCount=stoi(argv[2]);
    edgeCount=validate_graph_input();
    generate_random_graph();
    int source = rand()%vertexCount + 1;
    //print_verification_input(source);
    cout<<"Source : "<<source<<endl;
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    sssp(source);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout << "Total Execution Time : " << duration_cast<microseconds>(tend - tstart).count() << endl;
    return 0;
}