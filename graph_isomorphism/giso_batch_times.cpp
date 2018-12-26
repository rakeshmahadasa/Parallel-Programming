#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <chrono>

using namespace std;
using namespace std::chrono;





struct arg_struct {

	int node1;
	int node2;
	vector<int> g1v;
	vector<int> g2v;
	int g1c;
	int g2c;
	int tid;
	high_resolution_clock::time_point tstart;
};



int NUM_VERTICES;
int NUM_THREADS;
std::vector<std::vector<int> > g1(1000);
std::vector<std::vector<int> > g2(1000);
high_resolution_clock::time_point thread_begin_time[100000];
high_resolution_clock::time_point thread_end_time[100000];
vector<high_resolution_clock::time_point> bend(100000);
int thread_batch_map[100000];
bool giso=false;


bool graph_isomorphism(int node1,int node2,vector<int> g1v,vector<int> g2v,int g1c,int g2c){
	//cout<<node1<<" "<<node2<<endl;
	

	
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
			high_resolution_clock::time_point tend = high_resolution_clock::now();
			thread_end_time[args->tid]=tend;
			cout<<"Isomorphic : Total Execution Time : "<<duration_cast<seconds>( tend - args->tstart ).count()<<endl;
			pthread_exit(0);
		}
		else{
			high_resolution_clock::time_point tend = high_resolution_clock::now();
			thread_end_time[args->tid]=tend;
			pthread_exit(0);
		}
	}

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



int main(int argc,char**argv){
        //cin>>NUM_VERTICES>>NUM_THREADS;
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
			g2v[g2[0][j]]=0;
		}
		g1v[g1[0][i]]=0;
	}

	int batch=0;
	arg_index=0;
	int batch_count=0;
	bool batch_complete = false;
	high_resolution_clock::time_point tmain = high_resolution_clock::now();
	for(int i = 0; i<g1[0].size() && !giso;i++){
		for(int j=0;j<g2[0].size() && !giso;j++){
			batch++;
			arg_index++;
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			args[arg_index].tstart=t1;
			thread_begin_time[arg_index]=t1;
			thread_batch_map[arg_index]=batch_count+1;
			batch_complete=false;
			pthread_create(&threads[batch],NULL,parallel_isomorphism_check_util,&args[arg_index]);
			high_resolution_clock::time_point t_thread_start = high_resolution_clock::now();
			if(batch==NUM_THREADS){
				batch_count++;
				for(int k = 1;k<=batch;k++){
					pthread_join(threads[k],NULL);
				}
				batch=0;			
				high_resolution_clock::time_point t_thread_end = high_resolution_clock::now();
				int thread_batch_duration = duration_cast<microseconds>( t_thread_end - t_thread_start ).count();
				//cout <<"Batch Number : "<<batch_count<<" Execution Time : "<<thread_batch_duration<<endl;
				bend[batch_count]=t_thread_end;
				batch_complete=true;
			}
			
		}

	}
	if(!batch_complete){
		batch_count++;
		for(int k = 1;k<=batch;k++){
			pthread_join(threads[k],NULL);
		}
	}
	if(!giso){
		high_resolution_clock::time_point t2 = high_resolution_clock::now();

		int duration = duration_cast<microseconds>( t2 - tmain ).count();
		cout << "Graph Size: "<<NUM_VERTICES<<" Thread Count: "<<NUM_THREADS<<" Execution Time : "<<duration<<endl;

		int idx=1;
		for(int i = 1;i<=batch_count;i++){
			cout<<i<<" ";
			while(idx<=arg_index && thread_batch_map[idx]==i){

				if(thread_batch_map[idx] == batch_count && !batch_complete)
					cout<<-1<<" ";				
				else
					cout<<fixed<<(duration_cast<microseconds>(bend[thread_batch_map[idx]]-thread_end_time[idx]).count())/1000000.0<<" ";
				idx++;
			}
			cout<<endl;
		}


	}
	pthread_exit(0);
}
