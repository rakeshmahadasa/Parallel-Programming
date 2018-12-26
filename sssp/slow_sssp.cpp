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
long long int edgeCount=0;
vector<vector<pair<int,int>>> graph(10000001);
vector<int> dist;
vector<int> solution;
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
    if(edgeCount>(vertexCount*(vertexCount-1)/2)){
        cerr<<"Edge Count too high. Atmax "<<(vertexCount*(vertexCount-1)/2)<<" edges needed to build a fully connected graph with "<<vertexCount<<" vertices"<<endl;
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

long long int get_closest_to_source(){
    long long int closeVertexDist = MAX_DIST; 
    long long int closeVertexID = -1;
    for(int i = 1; i <= vertexCount ;i++){
        if(dist[i]!= -1 && dist[i]<closeVertexDist){
            closeVertexDist = dist[i];
            closeVertexID = i;
        }
    }
    return closeVertexID;
}

void sssp(int source){
    cout<<"Setting distance array"<<endl;
    dist.push_back(-1);
    for(int i = 1; i <= vertexCount;i++){
        dist.push_back(MAX_DIST);
        if(i == source) dist[i]=0;
        solution.push_back(MAX_DIST);
    }
    int heapSize=vertexCount;
    while(heapSize){
        int closeVertex = get_closest_to_source();
        int dist_su = dist[closeVertex];
        solution[closeVertex]=dist_su;
        dist[closeVertex]=-1;
        for(int i = 0; i < graph[closeVertex].size();i++){
            
            int dist_uv = graph[closeVertex][i].second;
            int curr_vertex = graph[closeVertex][i].first;
            int dist_sv = dist[curr_vertex];
            if( dist_sv > dist_su + dist_uv ){
                dist[curr_vertex]=dist_su + dist_uv;
            }
        }
        heapSize--;
    }

    for(int i = 1; i <=vertexCount; i++){
        cout<<i<<" "<<solution[i]<<endl;
    }

}
int main(int argc,char**argv){
    srand ( time(NULL) );
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
    print_verification_input(source);
    cout<<"Source : "<<source<<endl;
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    sssp(source);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout << "Total Execution Time : " << duration_cast<microseconds>(tend - tstart).count() << endl;
    return 0;
}