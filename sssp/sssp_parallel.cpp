#include <iostream>
#include <pthread.h>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

#ifndef MAX_DIST
#define MAX_DIST 1000000000
#endif

int NUM_THREADS = 7;
long long int vertexCount = 0;
long long int edgeCount = 0;
vector<vector<pair<int, int>>> graph(10000001);
vector<pair<int, int>> dist;
vector<int> indexMap;

pthread_cond_t dist_cond,index_cond;
pthread_mutex_t dist_mtx,index_mtx;

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 10000000
#endif

struct arg_struct
{
    int vertexID;
    int dist_uv;
    int dist_sv;
    int dist_su;
};

class bqueue
{
  private:
    struct arg_struct *worklist;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    int start;
    int end;

  public:
    int wcount;
    bqueue()
    {
        worklist = new arg_struct[QUEUE_SIZE];
        pthread_cond_init(&cond, NULL);
        pthread_mutex_init(&mtx, NULL);
        start = 0;
        end = 0;
        wcount=0;
    };

    void give_work(arg_struct a)
    {

        pthread_mutex_lock(&mtx);
        worklist[end] = a;
        end++;
        pthread_mutex_unlock(&mtx);
        pthread_cond_signal(&cond);
    }

    arg_struct take_work()
    {

        pthread_mutex_lock(&mtx);
        while (start == end){
            wcount++;
            int rand = pthread_cond_wait(&cond, &mtx);
            wcount--;
        }
        arg_struct args = worklist[start];
        start++;
        pthread_mutex_unlock(&mtx);
        return args;
    }

    ~bqueue()
    {

        delete[] worklist;
        pthread_mutex_destroy(&mtx);
        pthread_cond_destroy(&cond);
    };
};

void relax(int vertexID, int updateValue)
{
    pthread_mutex_lock(&dist_mtx);
    pthread_mutex_lock(&index_mtx);
    dist[indexMap[vertexID]].second = updateValue;
    pthread_mutex_unlock(&dist_mtx);
    pthread_mutex_unlock(&index_mtx);
}

void update_distance_parallel(int index)
{
    if (index <= 1)
        return;
    else
    {
        pthread_mutex_lock(&dist_mtx);
        pthread_mutex_lock(&index_mtx);
        while (index>1 && dist[index / 2].second > dist[index].second)
        {
            indexMap[dist[index].first] = index / 2;
            indexMap[dist[index / 2].first] = index;
            pair<int, int> temp = dist[index];
            dist[index] = dist[index / 2];
            dist[index / 2] = temp;
            index=index/2;
        }
        pthread_mutex_unlock(&dist_mtx);
        pthread_mutex_unlock(&index_mtx);
    }
}

void print_graph()
{
    for (int i = 1; i <= vertexCount; i++)
    {
        cout << i << " ";
        for (int j = 0; j < graph[i].size(); j++)
        {
            cout << graph[i][j].first << " ";
        }
        cout << endl;
    }
}
void print_graph_with_weights()
{
    for (int i = 1; i <= vertexCount; i++)
    {
        cout << i << " ";
        for (int j = 0; j < graph[i].size(); j++)
        {
            cout << "(" << graph[i][j].first << "," << graph[i][j].second << ")"
                 << " ";
        }
        cout << endl;
    }
}
void print_verification_input(int source)
{
    cout << vertexCount << " " << source - 1 << endl;
    for (int i = 1; i <= vertexCount; i++)
    {
        cout << graph[i].size() << " ";
        for (int j = 0; j < graph[i].size(); j++)
        {
            cout << graph[i][j].first - 1 << " " << graph[i][j].second << " ";
        }
        cout << endl;
    }
}
bool check_duplicate_edge(int e1, int e2)
{

    for (int i = 0; i < graph[e1].size(); i++)
    {
        if (graph[e1][i].first == e2)
            return true;
    }
    return false;
}
long long int validate_graph_input(){

    if(vertexCount<11){
        cerr<<"Vertex Count Must be greater than 10"<<endl;
        exit(0);
    }
    if(vertexCount>10000000){
        cerr<<"Vertex Count Must be less than 10000000"<<endl;
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
void generate_random_graph()
{

    for (int i = 1; i <= (vertexCount - 1); i++)
    {
        int randomWeight = rand() % 20 + 1;
        graph[i].push_back(make_pair(i + 1, randomWeight));
        graph[i + 1].push_back(make_pair(i, randomWeight));
    }
    int randomEdgeCount = 0;
    while (randomEdgeCount < (edgeCount - (vertexCount - 1)))
    {
        int e1 = rand() % vertexCount + 1;
        int e2 = rand() % vertexCount + 1;
        int randomWeight = rand() % 20 + 1;
        if (e1 != e2)
        {
            if (!check_duplicate_edge(e1, e2))
            {
                graph[e1].push_back(make_pair(e2, randomWeight));
                graph[e2].push_back(make_pair(e1, randomWeight));
                randomEdgeCount++;
            }
        }
    }
    //cout<<"Random graph generated "<<"vertex count : "<<vertexCount<<" Edge count : "<<edgeCount<<endl;
}
void update_distance(int index){
    if (index <= 1)
        return;
    else
    {
        if (dist[index / 2].second > dist[index].second)
        {
            indexMap[dist[index].first] = index / 2;
            indexMap[dist[index / 2].first] = index;
            pair<int, int> temp = dist[index];
            dist[index] = dist[index / 2];
            dist[index / 2] = temp;
            update_distance(index / 2);
        }
        else
        {
            return;
        }
    }
}
void heapify(int index, int heapSize)
{
    int left = 2 * index;
    int right = 2 * index + 1;
    int smallest = dist[index].second;
    int small_pos = index;
    if (left <= heapSize && dist[left].second < smallest)
    {
        smallest = dist[left].second;
        small_pos = left;
    }
    if (right <= heapSize && dist[right].second < smallest)
    {
        smallest = dist[right].second;
        small_pos = right;
    }
    if (smallest == dist[index].second)
    {
        return;
    }
    else
    {
        indexMap[dist[index].first] = small_pos;
        indexMap[dist[small_pos].first] = index;
        pair<int, int> temp = dist[index];
        dist[index] = dist[small_pos];
        dist[small_pos] = temp;
        heapify(small_pos, heapSize);
    }
}
void build_heap(int heapSize)
{
    for (int i = heapSize / 2; i >= 1; i--)
    {
        heapify(i, heapSize);
    }
}
void *parallel_dijkstra(void *input_args)
{
    while (true)
    {
        bqueue *work = (bqueue *)input_args;
        struct arg_struct args = work->take_work();
        if (args.vertexID == -1)
        {
            pthread_exit(0);
        }
        else
        {
            if (args.dist_sv > args.dist_su + args.dist_uv)
            {
                relax(args.vertexID, args.dist_su + args.dist_uv);
                update_distance_parallel(indexMap[args.vertexID]);
            }
        }
    }
}

void sssp(int source)
{
    dist.push_back(make_pair(0, MAX_DIST));
    indexMap.push_back(0);
    for (int i = 1; i <= vertexCount; i++)
    {
        dist.push_back(make_pair(i, MAX_DIST));
        if (i == source)
            dist[i].second = 0;
        indexMap.push_back(i);
    }
    build_heap(vertexCount);
    int heapSize = vertexCount;
    pthread_t threads[NUM_THREADS];
    pthread_cond_init(&dist_cond, NULL);
    pthread_mutex_init(&dist_mtx, NULL);
    pthread_cond_init(&index_cond, NULL);
    pthread_mutex_init(&index_mtx, NULL);

    bqueue *work = new bqueue();
    for (int i = 1; i <= NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, parallel_dijkstra, work);
    }
    while (heapSize)
    {
        pair<int, int> closeVertex = dist[1];
        indexMap[closeVertex.first] = heapSize;
        indexMap[dist[heapSize].first] = 1;
        pair<int, int> temp = dist[1];
        dist[1] = dist[heapSize];
        dist[heapSize] = temp;
        heapSize--;
        heapify(1, heapSize);
        for (int i = 0; i < graph[closeVertex.first].size(); i++)
        {
            struct arg_struct args;
            args.vertexID = graph[closeVertex.first][i].first;
            args.dist_uv = graph[closeVertex.first][i].second;
            int curr_vertex = graph[closeVertex.first][i].first;
            args.dist_sv = dist[indexMap[curr_vertex]].second;
            args.dist_su = dist[indexMap[closeVertex.first]].second;
            work->give_work(args);
        }
        while(work->wcount!=NUM_THREADS){}
        //heapSize--;
        //build_heap(heapSize);
    }
    for(int i=1;i<=NUM_THREADS;i++){
		struct arg_struct exitargs;
		exitargs.vertexID=-1;
		work->give_work(exitargs);
	}

	for(int i = 1;i<=NUM_THREADS;i++){
		pthread_join(threads[i],NULL);
	}

    delete work;
    pthread_mutex_destroy(&dist_mtx);
    pthread_cond_destroy(&dist_cond);
    pthread_mutex_destroy(&index_mtx);
    pthread_cond_destroy(&index_cond);

    // for(int i = 1; i <=vertexCount; i++){
    //     cout<<dist[indexMap[i]].first<<" "<<dist[indexMap[i]].second<<endl;
    // }
}
int main(int argc, char **argv)
{
    //srand ( time(NULL) );
    if (argc < 3)
    {
        cerr << "Required atleast two arguments ( vertexCount, thread count ). None provided" << endl;
        cerr << "Command : ./sssp <vertexCount> <thread count> <edgeCount-Optional>" << endl;
        exit(0);
    }
    vertexCount = stoi(argv[1]);
    NUM_THREADS = stoi(argv[2]); 
    edgeCount = -1;
    if (argc > 3)
        edgeCount = stoi(argv[3]);
    edgeCount = validate_graph_input();
    generate_random_graph();
    int source = rand() % vertexCount + 1;
    // print_verification_input(source);
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    sssp(source);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout << "Total Execution Time : " << duration_cast<microseconds>(tend - tstart).count() << endl;
    pthread_exit(0);
}