#include<iostream>
#include<pthread.h>
using namespace std;


class test{
    private:
        int memberVar=0;
    public:
        test(){
            memberVar=0;
        }
        void print_given(int arg){
            int curr = arg;
            memberVar=arg;
            if(curr!=arg)
                cout<<"Thread not safe : UnExpected"<<endl;
        }
};

void *parallel_dijkstra(void *input_args)
{
    int counter = 0;
    while (true)
    {
        test *work = (test *)input_args;
        work->print_given(counter);
        counter++;
    }
}

int main(){
    pthread_t threads[5];
    test *work = new test();
    for(int i =  0; i < 5;i++){
        pthread_create(&threads[i], NULL, parallel_dijkstra, work);
    }

    for (int i = 0; i < 5; i++)
    {
        pthread_join(threads[i], NULL);
    }


    return 0;
}