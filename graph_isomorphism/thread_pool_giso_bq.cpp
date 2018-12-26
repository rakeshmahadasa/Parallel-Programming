#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <chrono>
#include <queue>
using namespace std;
using namespace std::chrono;


#ifndef QUEUE_SIZE
#define QUEUE_SIZE 1000
#endif


struct arg_struct {

	int node1;
	int node2;
	vector<int> g1v;
	vector<int> g2v;
	int g1c;
	int g2c;
	int tid;
	bool work_done;
	high_resolution_clock::time_point tstart;
};


int NUM_VERTICES;
int NUM_THREADS;
std::vector<std::vector<int> > g1(1000);
std::vector<std::vector<int> > g2(1000);
queue<arg_struct> q;
bool giso_found=false;

class bqueue
{
private:
	struct arg_struct* worklist;
	pthread_cond_t cond;
	pthread_mutex_t mtx;
	int start;
	int end;

public:
	bqueue(){
		worklist = new arg_struct[QUEUE_SIZE];
		pthread_cond_init(&cond, NULL);
		pthread_mutex_init(&mtx, NULL);
		start=0;
		end=0;
	};


	void give_work(arg_struct a){

		pthread_mutex_lock(&mtx);
		worklist[end]=a;
		end++;
		pthread_mutex_unlock(&mtx);
		pthread_cond_signal(&cond);

	}


	arg_struct take_work(){

		pthread_mutex_lock(&mtx);
		while(start==end)
			int rand=pthread_cond_wait(&cond, &mtx);
		arg_struct args=worklist[start];
		start++;
		pthread_mutex_unlock(&mtx);
		return args;

	}

	~bqueue(){

		delete[] worklist;
		pthread_mutex_destroy(&mtx);
		pthread_cond_destroy(&cond);

	};

	
};



bool graph_isomorphism(int node1,int node2,vector<int> g1v,vector<int> g2v,int g1c,int g2c){
	if(g1c== 1 && g2c==1){
		bool g1_complete=true;
		bool g2_complete=true;
		for(int i = 0; i<=NUM_VERTICES;i++){
			if(g1v[i]==0){
				g1_complete=false;
				break;
			}
		}
		for(int i = 0; i<=NUM_VERTICES;i++){
			if(g2v[i]==0){
				g2_complete=false;
				break;
			}
		}
		if((g1[node1].size()==g2[node2].size()) && g1_complete && g2_complete){
			return true;
		}
		return false;
	}
	if(g1c== 1 || g2c==1){
		return false;
	}
	if(giso_found) return false;
	bool is_isomorphic=false;
	int n1_degree = g1[node1].size();
	int n2_degree = g2[node2].size();
	for(int i = 0; i<n1_degree;i++){
		int current_node1=g1[node1][i];
		if(g1v[current_node1]==0){
			g1v[current_node1]=1;
			for(int j=0;j<n2_degree;j++){
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

void skew_graph_generator_noniso(int NUM_VERTICES){


	for(int i = 1; i <=NUM_VERTICES;i++){
		g1[0].push_back(i);
		g2[0].push_back(i);
	}
	for (int i = 1; i <= NUM_VERTICES;i++){
		for(int j=i+1;j<= NUM_VERTICES;j++){
			if(i==1){
				g1[i].push_back(j);
				g1[j].push_back(i);
			}
			else if(!(i%2==1) && !(j%2==1)){
				g1[i].push_back(j);
				g1[j].push_back(i);
			}
			
		}
	}

	for (int i = 1; i <= NUM_VERTICES;i++){
		for(int j=i+1;j<= NUM_VERTICES;j++){
			g2[i].push_back(j);
			g2[j].push_back(i);
		}
	}

	// for(int i=1;i<=NUM_VERTICES;i++){
	// 	cout<<i<<" : ";
	// 	for(int j=0;j<g1[i].size();j++){
	// 		cout<<g1[i][j]<<" ";
	// 	}
	// 	cout<<endl;
	// }
}


void graph_generator_noniso(int NUM_VERTICES){


	for(int i = 1; i <=NUM_VERTICES;i++){
		g1[0].push_back(i);
		if (i!=NUM_VERTICES)
			g2[0].push_back(i);
	}
	for (int i = 1; i <= NUM_VERTICES;i++){
		for(int j=i;j<= NUM_VERTICES;j++){
			if(i!=j){
				g1[i].push_back(j);
				g1[j].push_back(i);

			}
		}
	}

	for (int i = 1; i < NUM_VERTICES;i++){
		for(int j=i;j< NUM_VERTICES;j++){
			if(i!=j){
				g2[i].push_back(j);
				g2[j].push_back(i);

			}
		}
	}
	


}



void *parallel_isomorphism_check_util(void* input_args){
	long long total_wait_time=0;
	int tasks=0;
	while(true){
		tasks++;
		bqueue* work = (bqueue *)input_args;		
		if(giso_found) pthread_exit(0);
		//high_resolution_clock::time_point before_wait = high_resolution_clock::now();
		struct arg_struct args = work->take_work();
		//high_resolution_clock::time_point after_wait = high_resolution_clock::now();
		//total_wait_time+=duration_cast<microseconds>( after_wait - before_wait ).count();
		if(args.work_done){
			//cout<<fixed<<"TotalTasks : "<<tasks<<" Thread Wait time "<<total_wait_time/1000000.0<<endl;
			pthread_exit(0);
		}
		//high_resolution_clock::time_point before_giso = high_resolution_clock::now();
		bool is_giso=graph_isomorphism(args.node1,args.node2,args.g1v,args.g2v,args.g1c,args.g2c);
		//high_resolution_clock::time_point after_giso = high_resolution_clock::now();
		//cout<<"Time taken for Giso : "<<duration_cast<microseconds>( after_giso - before_giso ).count()<<endl;
		if(is_giso){
			giso_found=true;
			high_resolution_clock::time_point tend = high_resolution_clock::now();
			cout<<"Isomorphic : Total Execution Time : "<<duration_cast<seconds>( tend - args.tstart ).count()<<endl;
			pthread_exit(0);
		}
	}
}






int main(int argc,char**argv){
	

	NUM_VERTICES=stoi(argv[1]);
	NUM_THREADS=stoi(argv[2]);
	skew_graph_generator_noniso(NUM_VERTICES);
	std::vector<int>g1v;
	std::vector<int>g2v;

	for(int i = 0; i <= NUM_VERTICES;i++){
		g1v.push_back(0);
		g2v.push_back(0);
	}
	g1v[0]=1;
	g2v[0]=1;

	pthread_t threads[NUM_THREADS];
	struct arg_struct args[g1[0].size()*g2[0].size()+1];
	int arg_index=0;
	for(int i = 0; i<g1[0].size();i++){
		int node1=g1[0][i];
		g1v[node1]=1;
		for(int j=0;j<g2[0].size();j++){
			int node2=g2[0][j];
			g2v[node2]=1;
			arg_index++;
			args[arg_index].g1v=g1v;
			args[arg_index].g2v=g2v;
			args[arg_index].g1c=NUM_VERTICES;
			args[arg_index].g2c=NUM_VERTICES-1;
			args[arg_index].node1=node1;
			args[arg_index].node2=node2;
			args[arg_index].tid=arg_index;
			args[arg_index].work_done=false;
			g2v[g2[0][j]]=0;
		}
		g1v[g1[0][i]]=0;
	}

	bqueue* work=new bqueue();

	for(int i =1;i<=NUM_THREADS;i++){
		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		args[arg_index].tstart=t1;		
		pthread_create(&threads[i],NULL,parallel_isomorphism_check_util,work);
	}
	arg_index=0;
	high_resolution_clock::time_point tmain = high_resolution_clock::now();
	for(int i = 0; i<g1[0].size() && !giso_found;i++){
		for(int j=0;j<g2[0].size() && !giso_found;j++){
			arg_index++;
			work->give_work(args[arg_index]);
		}
	}


	for(int i=1;i<=NUM_THREADS;i++){
		struct arg_struct exitargs;
		exitargs.work_done=true;
		work->give_work(exitargs);
	}
	for(int i = 1;i<=NUM_THREADS;i++){
		pthread_join(threads[i],NULL);
	}
	if(!giso_found){
		high_resolution_clock::time_point tcomplete = high_resolution_clock::now();
		int duration = duration_cast<microseconds>( tcomplete - tmain ).count();
		cout <<fixed<< "Graph Size: "<<NUM_VERTICES<<" Thread Count: "<<NUM_THREADS<<" Execution Time : "<<duration/1000000.0<<endl;
	}
	delete work;
	pthread_exit(0);
}
