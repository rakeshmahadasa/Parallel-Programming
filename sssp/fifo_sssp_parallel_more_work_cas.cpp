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
int vertexCount = 0;
lli edgeCount = 0;
vector<vector<pii>> graph(10000001);

vi dist;
pthread_mutex_t *vmtx;
int wait_counter = 0;
#ifndef QUEUE_SIZE
#define QUEUE_SIZE 10000001
#endif

template <class ET>
inline bool CAS(ET *ptr, ET oldv, ET newv) {
  if (sizeof(ET) == 1) {
    return __sync_bool_compare_and_swap((bool*)ptr, *((bool*)&oldv), *((bool*)&newv));
  } else if (sizeof(ET) == 4) {
    return __sync_bool_compare_and_swap((int*)ptr, *((int*)&oldv), *((int*)&newv));
  } else if (sizeof(ET) == 8) {
    return __sync_bool_compare_and_swap((long*)ptr, *((long*)&oldv), *((long*)&newv));
  }
  else {
    std::cout << "CAS bad length : " << sizeof(ET) << std::endl;
    abort();
  }
}

template <class ET>
inline ET writeAdd(ET *a, ET b) {
  volatile ET newV, oldV;
  do {oldV = *a; newV = oldV + b;}
  while (!CAS(a, oldV, newV));
  return newV;
}


class bqueue
{
  private:
    int *worklist;
    int start;
    int pivot;
    int end;

  public:
    bqueue()
    {
        worklist = new int[QUEUE_SIZE];
        start = 0;
        pivot=0;
        end = 0;
    };

    void give_work(int a)
    {
        int curr_end = writeAdd(&end,1);
        worklist[curr_end-1]=a;
        while (!CAS(&pivot, curr_end-1, curr_end ))
            sched_yield();
    }

    int take_work(int waitline)
    {
        while (true)
        {
            int curr_start = start;
            int curr_pivot = pivot;
            if (curr_start == curr_pivot)
            {  
                if(waitline)
                    writeAdd(&wait_counter,1);
                if(wait_counter == NUM_THREADS){
                    for(int i=0;i<NUM_THREADS-1;i++){                        
                        give_work(-1);
                    }
                    return -1;
                }
                return -2;
            }
            if(!waitline){
                writeAdd(&wait_counter,-1);
                waitline=true;
            }
            int args = worklist[curr_start];
            if (CAS(&start, curr_start, curr_start + 1))
            {
                return args;
            }
        }
    }

    ~bqueue()
    {

        delete[] worklist;
    };
};

void *parallel_dijkstra(void *input_args)
{
    bqueue *work = (bqueue *)input_args;
    while (true)
    {
        int u = work->take_work(true);
        while(u == -2){
            u = work->take_work(false);
        }
        if (u == -1)
        {
            pthread_exit(0);
        }
        else
        {
            //pthread_mutex_lock(&vmtx[u]);
            int dist_su = dist[u];
            for (int i = 0; i < graph[u].size(); i++)
            {
                int dist_uv = graph[u][i].second;
                int  v = graph[u][i].first;
                // if (v < u)
                //{
                //     pthread_mutex_unlock(&vmtx[u]);
                //     pthread_mutex_lock(&vmtx[u]);
                // }
                // else
                // {
                //     pthread_mutex_lock(&vmtx[v]);
                // }
                while (true)
                {
                    int dist_sv = dist[v];
                    bool committed = true;
                    if (dist_sv > dist_su + dist_uv)
                    {
                        committed = false;
                        if(CAS(&dist[v] ,dist_sv,dist_su + dist_uv))
                        // if (!is_in_workq[v])
                        // {
                        // is_in_workq[v] = true;
                            {
                                work->give_work(v);
                                committed=true;
                            }
                        // }
                    }
                    if(committed) break;
                }
            }
            // is_in_workq[u] = false;
            // pthread_mutex_unlock(&vmtx[u]);
        }
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
lli validate_graph_input()
{

    if (vertexCount < 11)
    {
        cerr << "Vertex Count Must be greater than 10" << endl;
        exit(0);
    }
    if (vertexCount > 10000000)
    {
        cerr << "Vertex Count Must be less than 20001" << endl;
        exit(0);
    }
    if (edgeCount == -1)
    {
        edgeCount = 5 * vertexCount - 1;
    }
    if (edgeCount < (vertexCount - 1))
    {
        cerr << "Edge Count too less. Atleast " << vertexCount - 1 << " edge needed to build a connected graph with " << vertexCount << " vertices" << endl;
        exit(0);
    }
    ulli max_possible_edge_count = (vertexCount * (vertexCount - 1)) / 2;
    if (edgeCount > max_possible_edge_count)
    {
        cerr << "Edge Count too high. At max " << (vertexCount * (vertexCount - 1) / 2) << " edges needed to build a fully connected graph with " << vertexCount << " vertices" << endl;
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

void sssp(int source)
{
    dist.push_back(-1);
    for (int i = 1; i <= vertexCount; i++)
    {
        dist.push_back(MAX_DIST);
    }
    dist[source] = 0;

    pthread_t threads[NUM_THREADS];
    bqueue *work = new bqueue();
    work->give_work(source);
    for (int i = 1; i <= NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, parallel_dijkstra, work);
    }
    for (int i = 1; i <= NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    
    delete[] vmtx;
    delete work;
    //  for(int i = 1; i <=vertexCount; i++){
    //      cout<<i<<" "<<dist[i]<<endl;
    //  }
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
    vmtx = new pthread_mutex_t[vertexCount + 1];
    NUM_THREADS = stoi(argv[2]);
    edgeCount = -1;
    if (argc > 3)
        edgeCount = stoi(argv[3]);

    edgeCount = validate_graph_input();
    generate_random_graph();
    //cout << "Vertex Count : " << vertexCount << " Edge Count : " << edgeCount << " Thread Count : " << NUM_THREADS << endl;
    int source = rand() % vertexCount + 1;
    //print_verification_input(source);
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    sssp(source);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout<<duration_cast<microseconds>(tend - tstart).count() <<endl;
    pthread_exit(0);
}