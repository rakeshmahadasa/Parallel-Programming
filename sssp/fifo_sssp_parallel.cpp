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

vi solution;
vector<bool> is_in_workq;
vi dist;
pthread_mutex_t *vmtx, dist_mtx;
pthread_barrier_t barrier;
int wait_counter = 0;
#ifndef QUEUE_SIZE
#define QUEUE_SIZE 10000001
#endif

class bqueue
{
  private:
    int *worklist;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    int start;
    int end;

  public:
    int wcount;
    bqueue()
    {
        worklist = new int[QUEUE_SIZE];
        pthread_cond_init(&cond, NULL);
        pthread_mutex_init(&mtx, NULL);
        start = 0;
        end = 0;
        wcount = 0;
    };

    int isempty(){
        return (end-start);
    }
    void give_work(int a)
    {

        pthread_mutex_lock(&mtx);
        worklist[end] = a;
        end++;
        pthread_mutex_unlock(&mtx);
        pthread_cond_signal(&cond);
    }

    int take_work()
    {

        pthread_mutex_lock(&mtx);

        while (start == end)
        {
            wait_counter++;
            //cout<<"WID : "<<wait_counter<<endl;
            if (wait_counter == NUM_THREADS)
            {
                pthread_mutex_unlock(&mtx);
                for (int i = 1; i < NUM_THREADS; i++)
                    give_work(-1);
                return -1;
            }
            int rand = pthread_cond_wait(&cond, &mtx);
            wait_counter--;
        }
        int args = worklist[start];
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

void *parallel_dijkstra(void *input_args)
{

    while (true)
    {
        bqueue *work = (bqueue *)input_args;
        int u = work->take_work();
        if (u == -1)
        {
            pthread_exit(0);
        }
        else
        {   
            pthread_mutex_lock(&vmtx[u]);
            int dist_su = dist[u];
            for (int i = 0; i < graph[u].size(); i++)
            {
                int dist_uv = graph[u][i].second;
                int v = graph[u][i].first;
                if (v < u)
                {
                    pthread_mutex_unlock(&vmtx[u]);
                    pthread_mutex_lock(&vmtx[v]);
                    pthread_mutex_lock(&vmtx[u]);
                    dist_su = dist[u];
                }
                else
                {
                    pthread_mutex_lock(&vmtx[v]);
                }
                int dist_sv = dist[v];
                is_in_workq[u] = false;
                if (dist_sv > dist_su + dist_uv)
                {
                    dist[v] = dist_su + dist_uv;
                    if (!is_in_workq[v])
                    {
                        is_in_workq[v] = true;
                        work->give_work(v);
                    }
                }
                pthread_mutex_unlock(&vmtx[v]);
            }
            pthread_mutex_unlock(&vmtx[u]);
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
    is_in_workq.push_back(false);
    pthread_mutex_init(&vmtx[0], NULL);
    pthread_mutex_init(&dist_mtx, NULL);
    for (int i = 1; i <= vertexCount; i++)
    {
        dist.push_back(MAX_DIST);
        solution.push_back(MAX_DIST);
        is_in_workq.push_back(false);
        pthread_mutex_init(&vmtx[i], NULL);
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
    cout<<"Items in work queue  : "<<work->isempty()<<endl;
    delete work;
    for(int i = 1; i <=vertexCount; i++){
        cout<<i<<" "<<dist[i]<<endl;
    }
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
    cout << "Vertex Count : " << vertexCount << " Edge Count : " << edgeCount << " Thread Count : " << NUM_THREADS << endl;
    int source = rand() % vertexCount + 1;
    //print_verification_input(source);
    high_resolution_clock::time_point tstart = high_resolution_clock::now();
    sssp(source);
    high_resolution_clock::time_point tend = high_resolution_clock::now();
    cout << "Total Execution Time : " << duration_cast<microseconds>(tend - tstart).count() << endl;
    pthread_exit(0);
}