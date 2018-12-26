#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

#define NUM_THREADS 50
#define NODE_COUNT 6

// std::vector<std::vector<int> > g1 { {1,2,3,4,5,6,7,8},
// { 5,6,7 },
// { 5,6,8 },
// { 5,7,8 },
// { 6,7,8 },
// { 1,2,3 },
// { 1,2,4 },
// { 1,3,4 },
// { 2,3,4 } };

// std::vector<std::vector<int> > g2 { {1,2,3,4,5,6,7,8},
// { 2,4,5 },
// { 1,3,6 },
// { 2,4,7 },
// { 1,3,8 },
// { 1,6,8 },
// { 2,5,7 },
// { 3,6,8 },
// { 4,5,7 } };

struct arg_struct {

	int node1;
	int node2;
	vector<int> g1v;
	vector<int> g2v;
	int g1c;
	int g2c;
	int tid;
	high_resolution_clock::time_point tstart;
	arg_struct() : g1v(7) ,g2v(7) {}
};


std::vector<std::vector<int> > g1 {{1,2,3,4,5,6},{4,5},{4,6},{5,6},{1,2},{1,3},{2,3}};
std::vector<std::vector<int> > g2 {{1,2,3,4,5,6},{3,5},{4,6},{1,5},{2,6},{1,3},{2,4}};


// std::vector<std::vector<int> > g1 {{1,2,3},{2},{1,3},{2}};
// std::vector<std::vector<int> > g2 {{1,2,3},{2,3},{1},{1}};

bool giso=false;
bool graph_isomorphism(int node1,int node2,vector<int> g1v,vector<int> g2v,int g1c,int g2c){
	//cout<<node1<<" "<<node2<<endl;
	

	
	if(g1c== 1 && g2c==1){
		return true;
	}

	if(g1c== 1 && g2c==1){
		return false;
	}
	
	if(giso) return false;

	bool is_isomorphic=false;

	for(int i = 0; i<g1[node1].size();i++){
		int current_node1=g1[node1][i];
		if(g1v[current_node1]==0){
			g1v[current_node1]=1;
			for(int j=0;j<g2[node2].size();j++){
				int current_node2=g2[node2][j];
				if(g2v[current_node2]==0){
					g2v[current_node2]=1;
					if(graph_isomorphism(current_node1,current_node2,g1v,g2v,g1c-1,g2c-1)){
						is_isomorphic=true;
						break;
					}
					g2v[current_node2]=0;
				}
			}
			if(is_isomorphic) return true;
			g1v[current_node1]=1;
		}
	}
	return false;
}

void *parallel_isomorphism_check_util(void* input_args){
	if (!giso){

		struct arg_struct *args = (struct arg_struct *)input_args;
		bool is_giso=graph_isomorphism(args->node1,args->node2,args->g1v,args->g2v,args->g1c,args->g2c);
		if(is_giso){
			giso=true;
		//cout<<args->tid<<" ISOMORPHIC : Start Nodes : "<<args->node1<<" "<<args->node2<<endl;
			high_resolution_clock::time_point tend = high_resolution_clock::now();
			cout<<"Isomorphic : Total Execution Time : "<<duration_cast<microseconds>( tend - args->tstart ).count()<<endl;
			pthread_exit(0);
		}
		else{
			pthread_exit(0);
		//cout<<args->tid<<" NOT ISOMORPHIC : Start Nodes : "<<args->node1<<" "<<args->node2<<endl;

		}
	}

}

int main(){
	std::vector<int>g1v={1,0,0,0,0,0,0};
	std::vector<int>g2v={1,0,0,0,0,0,0};
	pthread_t threads[NUM_THREADS];
	struct arg_struct args[NUM_THREADS];
	int batch=0;
	for(int i = 0; i<g1[0].size() && !giso;i++){
		int node1=g1[0][i];
		g1v[node1]=1;
		for(int j=0;j<g2[0].size() && !giso;j++){
			int node2=g2[0][j];
			g2v[node2]=1;
			batch++;
			args[batch].g1v=g1v;
			args[batch].g2v=g2v;
			args[batch].g1c=NODE_COUNT;
			args[batch].g2c=NODE_COUNT;
			args[batch].node1=node1;
			args[batch].node2=node2;
			args[batch].tid=batch;
			g2v[g2[0][j]]=0;
		}
		g1v[g1[0][i]]=0;
	}

	

	batch=0;
	high_resolution_clock::time_point tmain = high_resolution_clock::now();
	for(int i = 0; i<g1[0].size() && !giso;i++){
		for(int j=0;j<g2[0].size() && !giso;j++){
			batch++;
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			args[batch].tstart=t1;
			pthread_create(&threads[batch],NULL,parallel_isomorphism_check_util,&args[batch]);
			
		}
	}

	for(int k = 1;k<=batch;k++){
		pthread_join(threads[k],NULL);
	}

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	int duration = duration_cast<microseconds>( t2 - tmain ).count();
	cout << "Not Isomorphic : Total Execution Time : "<<duration<<endl;
	pthread_exit(0);
}
