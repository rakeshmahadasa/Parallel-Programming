//
//  GraphWithShortestPath.cpp
//  PageRankWithBarriers
//
//  Created by Mugilan M on 16/02/18.
//  Copyright © 2018 Mugilan M. All rights reserved.
//

#include "GraphWithShortestPath.hpp"
#include <queue>
#include "bitsetscheduler.hpp"
#include <pthread.h>
#include <unistd.h>
#include "rwlock.hpp"

#define LOCAL_UPPER_BOUND_UPDATE_INTERVAL 10000

typedef pair<uint32_t, uint32_t> uint32_tPair;
typedef pair<bool, uint32_t> boolIntPair;
typedef pair<uint32_t, boolIntPair> pqDistancePair;

struct argumentStructureForShortestPathComputeFunction{
    int threadId;
    bool usePriorityQueue;
    uint32_t sourceIndex, destinationIndex;
//    unsigned granularity;
    GraphWithShortestPath *myGraph;
};

typedef struct UpperBound{
    uint32_t distance;
    uint32_t joiningVertex;
}UpperBound;


typedef struct VertexStructure{
    uint32_t distanceToVertexFromSource;
    uint32_t predecessorFromSource;
    uint32_t distanceFromVertexToDestination;
    uint32_t predecessorFromDestination;
    bool shouldProcessOutEdges;
    bool shouldProcessInEdges;
    RWLock vertexLock;
    
    VertexStructure(){
        distanceToVertexFromSource = numeric_limits<uint32_t>::max();
        distanceFromVertexToDestination = numeric_limits<uint32_t>::max();
        predecessorFromSource = numeric_limits<uint32_t>::max();
        predecessorFromDestination = numeric_limits<uint32_t>::max();
        shouldProcessOutEdges = false;
        shouldProcessInEdges = false;
    }
    
    bool isVisitedFromSource(){
        return shouldProcessOutEdges;
    }
    
    bool isVisitedFromDestination(){
        return shouldProcessInEdges;
    }
    
    void setDistanceToVertexFromSource(uint32_t distance){
        distanceToVertexFromSource = distance;
        shouldProcessOutEdges = true;
    }
    
    void setDistanceFromVertexToDestination(uint32_t distance){
        distanceFromVertexToDestination = distance;
        shouldProcessInEdges = true;
    }
    
    void resetFlags(){
        shouldProcessOutEdges = false;
        shouldProcessInEdges = false;
    }
    
    void reset(){
        distanceToVertexFromSource = numeric_limits<uint32_t>::max();
        distanceFromVertexToDestination = numeric_limits<uint32_t>::max();
//        pathToVertexFromSource.clear();
//        pathToVertexFromDestination.clear();
        predecessorFromSource = numeric_limits<uint32_t>::max();
        predecessorFromDestination = numeric_limits<uint32_t>::max();
        shouldProcessOutEdges = false;
        shouldProcessInEdges = false;
    }
    
}VertexStructure;


vector<VertexStructure> allVertices;

//vector<RWLock> vertexLocks;
BitsetScheduler isNextVertexScheduled(0);

bool completedProcessing = false;
RWLock lockForCompletedProcessing;

vector<uint32_t> verticesToPreCompute;
unsigned nextVertexToPrecomputeIndex = 0;
pthread_mutex_t mutexForNextVertexToPreCompute;


pthread_mutex_t mutexForNextVertexQueue;
queue<uint32_t> nextVertexQueue;
//queue<uint32_tPair> nextVertexQueue;

pthread_mutex_t mutexForNextVertexPriotityQueue;
priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair>> nextVertexPriorityQueue;

TimerAggregator nextVertexQueueSizeAggregator, nextVertexQueueClearSizeAggregator, nextVertexQueueSchedueSizeAggregator;

typedef struct ConditionalWithCounter{
    int counter;
    pthread_cond_t conditional;
    pthread_mutex_t mutex;
    ConditionalWithCounter(){
        counter = 0;
    }
}ConditionalWithCounter;

//ConditionalWithCounter waitingForNextVertex = {0, NULL, NULL};
//ConditionalWithCounter completedProcessingConditional = {0, NULL, NULL};
ConditionalWithCounter waitingForNextVertex;
ConditionalWithCounter completedProcessingConditional;

atomic<UpperBound> upperBoundForShortestPath;
//atomic<uint32_t> upperBoundForShortestPath;
//atomic<uint32_t> joiningVertexForUpperBound;
//= numeric_limits<uint32_t>::max();
RWLock upperBoundForShortestPathRWlock;

atomic<uint64_t> globalNumberOfVerticesScheduled;
atomic<uint64_t> globalNumberOfVerticesToSchedule;
atomic<uint64_t> globalNumberOfVerticesComputed;
atomic<uint64_t> globalNumberOfVerticesIgnored;

void evaluateNeighbours(uint32_t currentVertexIndex, GraphWithShortestPath *myGraph, bool usePriorityQueue, Logger *debugLogger, vector<TimerAggregator> &timerAggregators, UpperBound &localUpperBoundForShortestPath, uint64_t &localNumberOfVerticesComputed, uint64_t &localNumberOfVerticesToSchedule, uint64_t &localNumberOfVerticesScheduled, uint32_t &localUpperBoundUpdateLastUpdatedCount);

void initializeMutexes(){
//    waitingForNextVertex.counter = 0;
    pthread_cond_init(&waitingForNextVertex.conditional, NULL);
    pthread_mutex_init(&waitingForNextVertex.mutex, NULL);
    pthread_cond_init(&completedProcessingConditional.conditional, NULL);
    pthread_mutex_init(&completedProcessingConditional.mutex, NULL);
    pthread_mutex_init(&mutexForNextVertexQueue, NULL);
    pthread_mutex_init(&mutexForNextVertexPriotityQueue, NULL);
    lockForCompletedProcessing.init();
    
    upperBoundForShortestPathRWlock.init();
    
    // Array of Structures implementation
    for (unsigned i=0; i<allVertices.size(); i++) {
        allVertices[i].vertexLock.init();
    }
}

void destroyMutexes(){
    pthread_cond_destroy(&waitingForNextVertex.conditional);
    pthread_mutex_destroy(&waitingForNextVertex.mutex);
    pthread_cond_destroy(&completedProcessingConditional.conditional);
    pthread_mutex_destroy(&completedProcessingConditional.mutex);
    pthread_mutex_destroy(&mutexForNextVertexQueue);
    pthread_mutex_destroy(&mutexForNextVertexPriotityQueue);
    lockForCompletedProcessing.destroy();
    // Array of Structures implementation
    for (unsigned i=0; i<allVertices.size(); i++) {
        allVertices[i].vertexLock.destroy();
    }
    upperBoundForShortestPathRWlock.destroy();
}

void resizeVectors(size_t size){
    isNextVertexScheduled.resize((uint32_t)size);
    isNextVertexScheduled.reset();
    
    allVertices.resize(size);
}

void resetFinalValues(){
    UpperBound temp = {numeric_limits<uint32_t>::max(), numeric_limits<uint32_t>::max()};
    upperBoundForShortestPath = temp;
//    upperBoundForShortestPath.joiningVertexForUpperBound = numeric_limits<uint32_t>::max();
}

void resetAllValues(){
    
    for (unsigned i = 0; i < allVertices.size(); i++) {
        allVertices[i].reset();
    }

    // FIFO queue implementation
    pthread_mutex_lock(&mutexForNextVertexQueue);
    isNextVertexScheduled.reset();
    while (!nextVertexQueue.empty()) {
        nextVertexQueue.pop();
    }
    pthread_mutex_unlock(&mutexForNextVertexQueue);

    // priority queue implementation
    pthread_mutex_lock(&mutexForNextVertexPriotityQueue);
    priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair>> temp;
    nextVertexPriorityQueue = temp;
    pthread_mutex_unlock(&mutexForNextVertexPriotityQueue);
    
    waitingForNextVertex.counter = 0;
    completedProcessing = false;
    
    globalNumberOfVerticesComputed = 0;
    globalNumberOfVerticesScheduled = 0;
    globalNumberOfVerticesToSchedule = 0;
    globalNumberOfVerticesIgnored = 0;
    
    nextVertexQueueSizeAggregator.reset();
    nextVertexQueueClearSizeAggregator.reset();
    nextVertexQueueSchedueSizeAggregator.reset();
}


bool getNextVertexToProcess(uint32_t &nextVertex){
    bool hasNextVertex = false;
    pthread_mutex_lock(&mutexForNextVertexQueue);
    if (!nextVertexQueue.empty()) {
        hasNextVertex = true;
        nextVertex = nextVertexQueue.front();
        nextVertexQueue.pop();
        isNextVertexScheduled.unschedule(nextVertex, true);
    }
    pthread_mutex_unlock(&mutexForNextVertexQueue);
    return hasNextVertex;
}

bool scheduleVertex(uint32_t vertexIndex){
    bool toReturn = false;
    pthread_mutex_lock(&mutexForNextVertexQueue);
    if (!isNextVertexScheduled.isScheduled(vertexIndex)) {
        toReturn = true;
        nextVertexQueue.push(vertexIndex);
        isNextVertexScheduled.schedule(vertexIndex, true);
        size_t count = nextVertexQueue.size();
        pthread_mutex_unlock(&mutexForNextVertexQueue);
        
        if (count == 1) {
            pthread_mutex_lock(&waitingForNextVertex.mutex);
            pthread_cond_broadcast(&waitingForNextVertex.conditional);
            pthread_mutex_unlock(&waitingForNextVertex.mutex);
        }
    }
    else{
        pthread_mutex_unlock(&mutexForNextVertexQueue);
    }
    return  toReturn;
}

bool getNextVertexToProcess_pq(uint32_t &nextVertex, uint32_t upperBound){
    bool nextVertexAvailable = false;
//    size_t sizeOfQueue;
    pthread_mutex_lock(&mutexForNextVertexPriotityQueue);
    if (!nextVertexPriorityQueue.empty()) {
//        sizeOfQueue = nextVertexPriorityQueue.size();
        unsigned currentVertexDistance = nextVertexPriorityQueue.top().first;
        if (currentVertexDistance >= upperBound) {
            // delete the entire priority queue
//            cout << "Before: " << nextVertexPriorityQueue.size()  <<"\n" <<endl;
            priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair>> temp;
            nextVertexPriorityQueue = temp;
//            nextVertexQueueClearSizeAggregator.see(sizeOfQueue);
//            cout << "After: " << nextVertexPriorityQueue.size()  <<"\n" <<endl;
            
        }
        else{
            nextVertexAvailable = true;
            nextVertex = nextVertexPriorityQueue.top().second;
            nextVertexPriorityQueue.pop();
//            nextVertexQueueSizeAggregator.see(sizeOfQueue);
        }
    }
    pthread_mutex_unlock(&mutexForNextVertexPriotityQueue);
    return nextVertexAvailable;
}



bool scheduleVertex_pq(uint32_t vertexIndex, uint32_t distance){
    bool toReturn = true;
    pthread_mutex_lock(&mutexForNextVertexPriotityQueue);
    // We cant do much about the duplicates in the priority queue.. Or can we?
    // Consider the vertex is already in the queue with distance d1.
    // When it is again scheduled with distance d2( d2 < d1 and d2 < upperBound ), if we don't schedule it again with distance d2.. When the top element is then popped and if its distance is lesser than upperBound then the entire queue is cleared.. In this case, we would have missed computing the vertex with distance d2 resulting in incorrect results.
    nextVertexPriorityQueue.push(make_pair(distance, vertexIndex));
    size_t count = nextVertexPriorityQueue.size();
    pthread_mutex_unlock(&mutexForNextVertexPriotityQueue);
    
//    nextVertexQueueSchedueSizeAggregator.see(count);
    if (count < 10) {
        pthread_mutex_lock(&waitingForNextVertex.mutex);
        pthread_cond_broadcast(&waitingForNextVertex.conditional);
        pthread_mutex_unlock(&waitingForNextVertex.mutex);
    }
    return toReturn;
}




//void lockVertex(uint32_t vertexIndex, bool writeLock){
//    if (writeLock) {
//        vertexLocks[vertexIndex].writeLock();
//    }
//    else{
//        vertexLocks[vertexIndex].readLock();
//    }
//}
//
//void unlockVertex(uint32_t vertexIndex){
//    vertexLocks[vertexIndex].unlock();
//}


void lockVertex(uint32_t vertexIndex, bool writeLock){
    if (writeLock) {
        allVertices[vertexIndex].vertexLock.writeLock();
    }
    else{
        allVertices[vertexIndex].vertexLock.readLock();
    }
}

void unlockVertex(uint32_t vertexIndex){
    allVertices[vertexIndex].vertexLock.unlock();
}

void readLockVertex(uint32_t vertexIndex){
    lockVertex(vertexIndex, false);
}

void writeLockVertex(uint32_t vertexIndex){
    lockVertex(vertexIndex, true);
}


uint32_t getNeighbouringVertex(GraphEdge edge, bool getSource){
    if (getSource) {
        return edge.getSourceVertex();
    }
    else{
        return edge.getDestinationVertex();
    }
}

bool hasCompletedProcessing(){
    bool toReturn = false;
    lockForCompletedProcessing.readLock();
    toReturn = completedProcessing;
    lockForCompletedProcessing.unlock();
    return toReturn;
}

void shortestPathBetween2VerticesComputeFunction3(unsigned threadId, void* arguments){
    // Function with threadPool
    struct argumentStructureForShortestPathComputeFunction *argumentStructure = (struct argumentStructureForShortestPathComputeFunction*) arguments;
    GraphWithShortestPath *myGraph = argumentStructure->myGraph;
    uint32_t sourceIndex = argumentStructure->sourceIndex;
    uint32_t destinationIndex = argumentStructure->destinationIndex;
    string queryString = to_string(sourceIndex);
    queryString = queryString + "->" + to_string(destinationIndex);
    
    bool usePriorityQueue = argumentStructure->usePriorityQueue;
    CSVRow logRow, logRow1, logRow2, logRow3, logRow4;
    logRow3.append(queryString);
    
    uint32_t currentVertexIndex;
    uint64_t localNumberOfVerticesScheduled = 0;
    uint64_t localNumberOfVerticesToSchedule = 0;
    uint64_t localNumberOfVerticesComputed = 0;
    
    Logger debugLogger;
    debugLogger.attachLogFile(Utils::getOutputFolderPath() + to_string(threadId) + "_threadLog.csv");
    debugLogger.setShouldPrintToConsole(false);

    uint64_t nextVertexTimer, totalThreadTime=Utils::getDebugTimer(), waitingForNextVertexTimer;
    
    TimerAggregator nextVertexTimerAggregator, waitingForNextVertexTimerAggregator;
    vector<TimerAggregator> timerAggregators(3);
    
    UpperBound localUpperBoundForShortestPath = upperBoundForShortestPath , prevUpperBound = upperBoundForShortestPath;
    uint32_t localUpperBoundUpdateLastUpdatedCount = 0;
//    uint32_t localUpperBoundForShortestPath = numeric_limits<uint32_t>::max();
    bool nextVertexIsAvailable = false;
    uint32_t count = 0;
    uint64_t timer = Utils::getTimer();
    
    while(true){
        
        nextVertexTimer = Utils::getDebugTimer();
        if (usePriorityQueue) {
            nextVertexIsAvailable = getNextVertexToProcess_pq(currentVertexIndex, localUpperBoundForShortestPath.distance);
        }
        else{
            nextVertexIsAvailable = getNextVertexToProcess(currentVertexIndex);
        }
        
        
        nextVertexTimer = Utils::getDebugTimer() - nextVertexTimer;
        nextVertexTimerAggregator.add(nextVertexTimer);
        if(!nextVertexIsAvailable){
            
            if (hasCompletedProcessing()) {
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                totalThreadTime = Utils::getDebugTimer() - totalThreadTime;
                
                globalNumberOfVerticesToSchedule += localNumberOfVerticesToSchedule;
                globalNumberOfVerticesScheduled += localNumberOfVerticesScheduled;
                globalNumberOfVerticesComputed += localNumberOfVerticesComputed;
                
                logRow3.append(to_string(localNumberOfVerticesToSchedule));
                logRow3.append(to_string(localNumberOfVerticesScheduled));
                logRow3.append(to_string(localNumberOfVerticesComputed));
                
//                debugLogger.writeLog(queryString);
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                debugLogger.writeLog(logRow3.getString());
                debugLogger.writeLog(logRow4.getString());
                debugLogger.writeLog(logRow2.getString());
                debugLogger.writeLog(logRow1.getString());
                
//                if (usePriorityQueue) {
//                    logRow.append("PQ");
//                }
//                else{
//                    logRow.append("FIFO");
//                }
//                logRow.append("FINAL");
//                logRow.append(to_string(totalThreadTime));
//                logRow.append(nextVertexTimerAggregator.totalValue());
//                logRow.append(waitingForNextVertexTimerAggregator.totalValue());
//                for (unsigned i=0; i<timerAggregators.size(); i++) {
//                    logRow.append(timerAggregators[i].totalValue());
//                }
//                debugLogger.writeLog(logRow.getString());
//                logRow.clear();
//
//
//                if (usePriorityQueue) {
//                    logRow.append("PQ");
//                }
//                else{
//                    logRow.append("FIFO");
//                }
//                logRow.append("FINAL - detailed");
//                logRow.append(to_string(totalThreadTime));
//                logRow.append(nextVertexTimerAggregator.finalValue());
//                logRow.append(waitingForNextVertexTimerAggregator.finalValue());
//                for (unsigned i=0; i<timerAggregators.size(); i++) {
//                    logRow.append(timerAggregators[i].finalValue());
//                }
//                debugLogger.writeLog(logRow.getString());
                return;
                // sync threadPool
                // the thread will now start waiting at this barrier point
                //                continue;
            }
            
            // wait till new vertex is added
            pthread_mutex_lock(&waitingForNextVertex.mutex);
            waitingForNextVertex.counter++;
            if (waitingForNextVertex.counter == Utils::numberOfThreads) {
                
                waitingForNextVertex.counter--;
                
                // Update the completedProcessing flag
                lockForCompletedProcessing.writeLock();
                completedProcessing = true;
                lockForCompletedProcessing.unlock();
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                
                // We will also signal all threads waiting for the next vertex
                pthread_cond_broadcast(&waitingForNextVertex.conditional);
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                
            }
            else{
                struct timespec max_wait = Utils::getTimeSpec(2);
                waitingForNextVertexTimer = Utils::getDebugTimer();
                pthread_cond_timedwait(&waitingForNextVertex.conditional, &waitingForNextVertex.mutex, &max_wait);
                waitingForNextVertex.counter--;
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                waitingForNextVertexTimer = Utils::getDebugTimer() - waitingForNextVertexTimer;
                waitingForNextVertexTimerAggregator.add(waitingForNextVertexTimer);
            }
            continue;
        }
        
        
        
        count++;
//        if (count%1000) {
//            logRow.append(to_string(localUpperBoundForShortestPath.distance));
//            logRow1.append(to_string(Utils::getTimer() - timer));
            if (prevUpperBound.distance != localUpperBoundForShortestPath.distance) {
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                prevUpperBound = localUpperBoundForShortestPath;
            }
//        }

        evaluateNeighbours(currentVertexIndex, myGraph, usePriorityQueue, &debugLogger, timerAggregators, localUpperBoundForShortestPath, localNumberOfVerticesComputed,  localNumberOfVerticesToSchedule, localNumberOfVerticesScheduled, localUpperBoundUpdateLastUpdatedCount);
        
    }
    return;
}


void evaluateNeighbours(uint32_t currentVertexIndex, GraphWithShortestPath *myGraph, bool usePriorityQueue, Logger *debugLogger, vector<TimerAggregator> &timerAggregators, UpperBound &localUpperBoundForShortestPath, uint64_t &localNumberOfVerticesComputed, uint64_t &localNumberOfVerticesToSchedule, uint64_t &localNumberOfVerticesScheduled, uint32_t &localUpperBoundUpdateLastUpdatedCount){

    
    uint32_t currentVertexDistanceFromSource, currentVertexDistanceToDestination;
    TimerAggregator &upperBoundRLockTimerAggregator = timerAggregators[0];
    TimerAggregator &upperBoundWLockTimerAggregator = timerAggregators[1];
    TimerAggregator &scheduleVertexTimerAggregator = timerAggregators[2];
    uint64_t upperBoundRLockTimer;

    
    // cant lock the vertex for the entire duration as it will create deadlock
    bool shouldEvaluateOutEdges;
    bool shouldEvaluateInEdges;

    writeLockVertex(currentVertexIndex);
    currentVertexDistanceFromSource = allVertices[currentVertexIndex].distanceToVertexFromSource;
    currentVertexDistanceToDestination = allVertices[currentVertexIndex].distanceFromVertexToDestination;
    shouldEvaluateOutEdges = allVertices[currentVertexIndex].isVisitedFromSource();
    shouldEvaluateInEdges = allVertices[currentVertexIndex].isVisitedFromDestination();
    allVertices[currentVertexIndex].resetFlags();
    unlockVertex(currentVertexIndex);
    
    if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
        upperBoundRLockTimer = Utils::getDebugTimer();
        localUpperBoundForShortestPath = upperBoundForShortestPath;
        upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
        upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
        localUpperBoundUpdateLastUpdatedCount = 0;
    }
    
    if (shouldEvaluateOutEdges && (currentVertexDistanceFromSource < localUpperBoundForShortestPath.distance)) {
        shouldEvaluateOutEdges = true;
    }
    if (shouldEvaluateInEdges && (currentVertexDistanceToDestination < localUpperBoundForShortestPath.distance)) {
        shouldEvaluateInEdges = true;
    }

    if (!shouldEvaluateInEdges && !shouldEvaluateOutEdges) {
        return;
    }
    
    localNumberOfVerticesComputed++;
    
    GraphVertex *currentVertex;
    vector<GraphEdge> inEdges, outEdges;
    
    readLockVertex(currentVertexIndex);
    currentVertex = &myGraph->allVerticesVector[currentVertexIndex];
    if (shouldEvaluateOutEdges) {
        outEdges = currentVertex->outEdges;
    }
    if (shouldEvaluateInEdges){
        inEdges = currentVertex->inEdges;
    }
    unlockVertex(currentVertexIndex);
    
    upperBoundRLockTimer = Utils::getDebugTimer();
    
    if (shouldEvaluateOutEdges) {
        for (GraphEdge &edge : outEdges) {
            uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
            
            uint64_t upperBoundWLockTimer, scheduleVertexTimer;
            
            // lock and get the neighbouring Vertex values;
            writeLockVertex(neighbouringVertexIndex);
            bool neighbouringVertexVisitedFromDestinationFlag = allVertices[neighbouringVertexIndex].isVisitedFromDestination();
            uint32_t neighbouringVertexDistanceFromSource = allVertices[neighbouringVertexIndex].distanceToVertexFromSource;
            uint32_t neighbouringVertexDistanceToDestination = allVertices[neighbouringVertexIndex].distanceFromVertexToDestination;
            uint32_t distanceToReachNeighbouringVertexThroughCurrentVertex = currentVertexDistanceFromSource + (uint32_t)edge.weight;

            // This seems like a good way to reduce computation
//            upperBoundRLockTimer = Utils::getDebugTimer();
//            UpperBound tempUpperBound = upperBoundForShortestPath;
//            upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
//            upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
            
            if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
                upperBoundRLockTimer = Utils::getDebugTimer();
                localUpperBoundForShortestPath = upperBoundForShortestPath;
                upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                localUpperBoundUpdateLastUpdatedCount = 0;
            }
            
            if (localUpperBoundForShortestPath.distance <= distanceToReachNeighbouringVertexThroughCurrentVertex) {
                // We can just quit here as any path through neighBouringVertex is going to be of greater distance than upperBound
                unlockVertex(neighbouringVertexIndex);
                continue;
            }
            
            if (neighbouringVertexVisitedFromDestinationFlag) {
                // In this case, the neighbouringVertex is already visited from destination.
                // So, a joining point has been reached.
                // path = pathFromSourceToCurrentVertex + pathFromNeighBouringVertexToDestination
                // pathDistance = distanceToCurrentVertexFromSource + distanceFromNeighbouringVertexToDestination + edge.weight
                
                uint32_t finalDistance = currentVertexDistanceFromSource + neighbouringVertexDistanceToDestination + (uint32_t)edge.weight;
                
                upperBoundWLockTimer = Utils::getDebugTimer();
//                upperBoundForShortestPathRWlock.writeLock();
//                if (finalDistance < upperBoundForShortestPath) {
//                    pathCorrespondingToUpperBound = finalPath;
//                    upperBoundForShortestPath = finalDistance;
//                    localUpperBoundForShortestPath = finalDistance;
//                }
//                upperBoundForShortestPathRWlock.unlock();

                UpperBound currentValue;
                currentValue.distance = finalDistance;
                currentValue.joiningVertex = neighbouringVertexIndex;
                localUpperBoundForShortestPath = currentValue;
                localUpperBoundUpdateLastUpdatedCount = 0;
                UpperBound prev_value = upperBoundForShortestPath;
                while (true) {
                    if (currentValue.distance > prev_value.distance) {
                        upperBoundRLockTimer = Utils::getDebugTimer();
                        localUpperBoundForShortestPath = upperBoundForShortestPath;
                        upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                        upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                        break;
                    }
                    if (upperBoundForShortestPath.compare_exchange_weak(prev_value, currentValue)) {
                        break;
                    }
                }
                
                upperBoundWLockTimer = Utils::getDebugTimer() - upperBoundWLockTimer;
                upperBoundWLockTimerAggregator.add(upperBoundWLockTimer);
            }
            
            // Update the path taken to reach the neighbouringVertex if the distance of the path through the currentVertex is lesser than the distance of the already computed path of the neighbouringVertex
            if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceFromSource) {
                
                allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(distanceToReachNeighbouringVertexThroughCurrentVertex);
                allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
                
                localNumberOfVerticesToSchedule++;
                if (distanceToReachNeighbouringVertexThroughCurrentVertex < localUpperBoundForShortestPath.distance) {
                    scheduleVertexTimer = Utils::getDebugTimer();
                    if (usePriorityQueue) {
                        if(scheduleVertex_pq(neighbouringVertexIndex, distanceToReachNeighbouringVertexThroughCurrentVertex))
                            localNumberOfVerticesScheduled++;
                    }
                    else{
                        if(scheduleVertex(neighbouringVertexIndex))
                            localNumberOfVerticesScheduled++;
                    }
                    scheduleVertexTimer = Utils::getDebugTimer() - scheduleVertexTimer;
                    scheduleVertexTimerAggregator.add(scheduleVertexTimer);
                }
            }

            unlockVertex(neighbouringVertexIndex);
        }
    }
    
    if (shouldEvaluateInEdges){
        for (GraphEdge &edge : inEdges) {
            
            uint64_t upperBoundRLockTimer, upperBoundWLockTimer, scheduleVertexTimer;
            uint32_t neighbouringVertexIndex = edge.getSourceVertex();
            // lock and get the neighbouring Vertex values;
            writeLockVertex(neighbouringVertexIndex);

            bool neighbouringVertexVisitedFromSourceFlag = allVertices[neighbouringVertexIndex].isVisitedFromSource();
            uint32_t neighbouringVertexDistanceFromSource = allVertices[neighbouringVertexIndex].distanceToVertexFromSource;
            uint32_t neighbouringVertexDistanceToDestination = allVertices[neighbouringVertexIndex].distanceFromVertexToDestination;
            uint32_t distanceToReachNeighbouringVertexThroughCurrentVertex = currentVertexDistanceToDestination + (uint32_t)edge.weight;
           
            if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
                upperBoundRLockTimer = Utils::getDebugTimer();
                localUpperBoundForShortestPath = upperBoundForShortestPath;
                upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                localUpperBoundUpdateLastUpdatedCount = 0;
            }

            
            if (localUpperBoundForShortestPath.distance <= distanceToReachNeighbouringVertexThroughCurrentVertex) {
                // We can just quit here as any path through neighBouringVertex is going to be of greater distance than upperBound
                unlockVertex(neighbouringVertexIndex);
                continue;
            }

            if (neighbouringVertexVisitedFromSourceFlag) {
                // In this case, the neighbouringVertex is already visited from source.
                // So, a joining point has been reached.
                // path = pathFromSourceToNeighbouringVertex + pathFromCurrentVertexToDestination
                // pathDistance = distanceToNeighbouringVertexFromSource + distanceFromCurrentVertexToDestination + edge.weight
                
                uint32_t finalDistance = neighbouringVertexDistanceFromSource + currentVertexDistanceToDestination + (uint32_t)edge.weight;
                
                upperBoundWLockTimer = Utils::getDebugTimer();
//                upperBoundForShortestPathRWlock.writeLock();
//                if (finalDistance < upperBoundForShortestPath) {
//                    pathCorrespondingToUpperBound = finalPath;
//                    upperBoundForShortestPath = finalDistance;
//                    localUpperBoundForShortestPath = finalDistance;
//                }
//                upperBoundForShortestPathRWlock.unlock();

                UpperBound currentValue;
                currentValue.distance = finalDistance;
                currentValue.joiningVertex = neighbouringVertexIndex;
                localUpperBoundForShortestPath = currentValue;
                localUpperBoundUpdateLastUpdatedCount = 0;

                UpperBound prev_value = upperBoundForShortestPath;
                while (true) {
                    if (finalDistance > prev_value.distance) {
                        // Update localUpperBound
                        upperBoundRLockTimer = Utils::getDebugTimer();
                        localUpperBoundForShortestPath = upperBoundForShortestPath;
                        upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                        upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                        break;
                    }
                    if (upperBoundForShortestPath.compare_exchange_weak(prev_value, currentValue)) {
                        break;
                    }
                }
                
                upperBoundWLockTimer = Utils::getDebugTimer() - upperBoundWLockTimer;
                upperBoundWLockTimerAggregator.add(upperBoundWLockTimer);
            }

            // Update the path taken to reach the neighbouringVertex if the distance of the path through the currentVertex is lesser than the distance of the already computed path of the neighbouringVertex
            if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceToDestination) {
                allVertices[neighbouringVertexIndex].setDistanceFromVertexToDestination(distanceToReachNeighbouringVertexThroughCurrentVertex);
                allVertices[neighbouringVertexIndex].predecessorFromDestination = currentVertexIndex;

                localNumberOfVerticesToSchedule++;
                if (distanceToReachNeighbouringVertexThroughCurrentVertex < localUpperBoundForShortestPath.distance) {
                    scheduleVertexTimer = Utils::getDebugTimer();
                    if (usePriorityQueue) {
                        if(scheduleVertex_pq(neighbouringVertexIndex, distanceToReachNeighbouringVertexThroughCurrentVertex))
                            localNumberOfVerticesScheduled++;
                    }
                    else{
                        if(scheduleVertex(neighbouringVertexIndex))
                            localNumberOfVerticesScheduled++;
                    }
                    scheduleVertexTimer = Utils::getDebugTimer() - scheduleVertexTimer;
                    scheduleVertexTimerAggregator.add(scheduleVertexTimer);
                }
            }
            unlockVertex(neighbouringVertexIndex);
        }
    }
}


void GraphWithShortestPath::initializeShortestPathValues(){
    resizeVectors(allVerticesVector.size());
    initializeMutexes();
}

void GraphWithShortestPath::destroyShortestPathValues(){
    destroyMutexes();
}

ShortestPath GraphWithShortestPath::getShortestPath3(uint32_t sourceIndex, uint32_t destinationIndex, bool usePriorityQueue){
    ShortestPath toReturn;
    toReturn.distance = numeric_limits<uint32_t>::max();
    
    CSVRow logRow;
    logRow.append("PARENT");
    
    
    //    uint64_t resetTimer=Utils::getDebugTimer();
    resetAllValues();
    //    resetTimer = Utils::getDebugTimer() - resetTimer;
    //    logRow.append(to_string(resetTimer));
    //
    
    // Array of structures
    allVertices[sourceIndex].setDistanceToVertexFromSource(0);
    allVertices[destinationIndex].setDistanceFromVertexToDestination(0);
    
    if(usePriorityQueue){
        scheduleVertex_pq(sourceIndex, 0);
        scheduleVertex_pq(destinationIndex, 0);
    }
    else{
        scheduleVertex(sourceIndex);
        scheduleVertex(destinationIndex);
    }
    // update threadInfo arguments
    for (unsigned i=0; i<Utils::getNumberOfThreads(); i++) {
        struct argumentStructureForShortestPathComputeFunction *arg = (struct argumentStructureForShortestPathComputeFunction*)threadPool.getThreadInfoArgument(i);
        arg->usePriorityQueue = usePriorityQueue;
        arg->sourceIndex = sourceIndex;
        arg->destinationIndex = destinationIndex;
        threadPool.setThreadInfoArgument(i, arg);
    }

    
    globalNumberOfVerticesScheduled +=2;
    globalNumberOfVerticesToSchedule +=2;
    
    threadPool.perform(&shortestPathBetween2VerticesComputeFunction3);
    
    lockForCompletedProcessing.readLock();
    if (completedProcessing == false) {
        pthread_mutex_lock(&completedProcessingConditional.mutex);
        lockForCompletedProcessing.unlock();
        pthread_cond_wait(&completedProcessingConditional.conditional, &completedProcessingConditional.mutex);
        pthread_mutex_unlock(&completedProcessingConditional.mutex);
    }
    else{
        lockForCompletedProcessing.unlock();
    }
    
    threadPool.sync();
    
    UpperBound tempUpperBound = upperBoundForShortestPath;
    toReturn.distance = tempUpperBound.distance;
    toReturn.joiningVertex = tempUpperBound.joiningVertex;
    logRow.append(to_string(globalNumberOfVerticesToSchedule));
    logRow.append(to_string(globalNumberOfVerticesScheduled));
    logRow.append(to_string(globalNumberOfVerticesComputed));
//    logRow.append(nextVertexQueueSizeAggregator.meanValue());
//    logRow.append(nextVertexQueueClearSizeAggregator.meanValue());
//    logRow.append(nextVertexQueueSchedueSizeAggregator.meanValue());
    Utils::logCSVRow(DEBUG_LOG, logRow);
    
//    logRow.clear();
//    logRow.append("");
//    logRow.append("");
//    logRow.append("");
//    logRow.append("");
//    logRow.append(nextVertexQueueSizeAggregator.finalValue());
//    logRow.append(nextVertexQueueClearSizeAggregator.finalValue());
//    logRow.append(nextVertexQueueSchedueSizeAggregator.finalValue());
//    Utils::logCSVRow(DEBUG_LOG, logRow);
    
//    cout << "completed\n" <<endl;
    
    return toReturn;
}

ShortestPath GraphWithShortestPath::getShortestPath3(uint32_t sourceIndex, uint32_t destinationIndex){
    return getShortestPath3(sourceIndex, destinationIndex, false);
}


GraphWithShortestPath::GraphWithShortestPath(unsigned nThreads) : Graph(), threadPool(nThreads) {
    for (unsigned i=0; i<nThreads; i++) {
        struct argumentStructureForShortestPathComputeFunction *arg = new struct argumentStructureForShortestPathComputeFunction;
        arg->threadId = i;
        arg->myGraph = this;
        threadPool.setThreadInfoArgument(i, arg);
    }
    threadPool.createPool();
}

ShortestPath GraphWithShortestPath::getShortestPathWithBounds(uint32_t source, uint32_t destination, bool usePriorityQueue, bool useUnidirectional, uint32_t &upperBoundToReturn){
    uint32_t upperBound = numeric_limits<uint32_t>::max();
    uint32_t joiningVertex = numeric_limits<uint32_t>::max();
    //    vector<uint32_t> upperBoundpath;
    
    
    CSVRow logRow;
    logRow.append("MAIN");
    // reset upperBound and get the time
    
    resetFinalValues();
    
    uint64_t totalExecutionTime = Utils::getTimer();
    uint64_t upperBoundTimer=Utils::getTimer();
    for (auto& mapEntry : precomputedIncomingShortestPathsMap) {
        uint32_t currentVertexIndex = mapEntry.first;
        uint32_t distanceFromSource = precomputedIncomingShortestPathsMap[currentVertexIndex]->allShortestPaths[source].distance;
        if (distanceFromSource < upperBound) {
            uint32_t distanceToDestination = precomputedOutgoingShortestPathsMap[currentVertexIndex]->allShortestPaths[destination].distance;
            if (distanceToDestination < upperBound) {
                // update upperBound
                upperBound = distanceFromSource + distanceToDestination;
                joiningVertex = currentVertexIndex;
                //                vector<uint32_t> incomingPath = precomputedIncomingShortestPathsMap[currentVertexIndex]->allShortestPaths[source].path;
                //                vector<uint32_t> outGoingPath = precomputedOutgoingShortestPathsMap[currentVertexIndex]->allShortestPaths[destination].path;
                //                upperBoundpath.clear();
                //                upperBoundpath.insert(upperBoundpath.end(), incomingPath.rbegin(), incomingPath.rend()-1);
                //                upperBoundpath.insert(upperBoundpath.end(), outGoingPath.begin(), outGoingPath.end());
            }
        }
    }
    upperBoundTimer = Utils::getTimer() - upperBoundTimer;
    logRow.append(to_string(upperBound));
    logRow.append(to_string(upperBoundTimer));
    
    // update global upperbound
    UpperBound tempUpperBound;
    tempUpperBound.distance = upperBound;
    tempUpperBound.joiningVertex = joiningVertex;
    upperBoundForShortestPath = tempUpperBound;
    upperBoundToReturn = tempUpperBound.distance;
    //    pathCorrespondingToUpperBound = upperBoundpath;
    
    ShortestPath sp;
    if (useUnidirectional) {
        sp = getShortestPath_parallel_unidirectional(source, destination, usePriorityQueue);
    }
    else{
        sp = getShortestPath3(source, destination, usePriorityQueue);
    }
    
    
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp.distance));
    logRow.append(to_string(totalExecutionTime));
    
    //    Utils::logCSVRow(DEBUG_LOG, logRow);
    
    return sp;
}
    
    

ShortestPath GraphWithShortestPath::getShortestPathWithBounds(uint32_t source, uint32_t destination, uint32_t &upperBoundToReturn){
    return getShortestPathWithBounds(source, destination, false, true, upperBoundToReturn);
}

ShortestPath GraphWithShortestPath::getShortestPath_all(uint32_t source, uint32_t destination){
    uint32_t upperBound = numeric_limits<uint32_t>::max();
    vector<uint32_t> upperBoundpath;
    
    CSVRow logRow;
    logRow.append("MAIN");
    // reset upperBound and get the time
    
    resetFinalValues();
    UpperBound temp;
    temp.joiningVertex = numeric_limits<uint32_t>::max();
    
//    upperBoundForShortestPath = numeric_limits<uint32_t>::max();
//    pathCorrespondingToUpperBound.clear();
    
    uint64_t totalExecutionTime = Utils::getTimer();
    uint64_t upperBoundTimer=Utils::getTimer();
    for (auto& mapEntry : precomputedIncomingShortestPathsMap) {
        uint32_t currentVertexIndex = mapEntry.first;
        uint32_t distanceFromSource = precomputedIncomingShortestPathsMap[currentVertexIndex]->allShortestPaths[source].distance;
        if (distanceFromSource < upperBound) {
            uint32_t distanceToDestination = precomputedOutgoingShortestPathsMap[currentVertexIndex]->allShortestPaths[destination].distance;
            if (distanceToDestination < upperBound) {
                // update upperBound
                upperBound = distanceFromSource + distanceToDestination;
//                vector<uint32_t> incomingPath = precomputedIncomingShortestPathsMap[currentVertexIndex]->allShortestPaths[source].path;
//                vector<uint32_t> outGoingPath = precomputedOutgoingShortestPathsMap[currentVertexIndex]->allShortestPaths[destination].path;
//                upperBoundpath.clear();
//                upperBoundpath.insert(upperBoundpath.end(), incomingPath.rbegin(), incomingPath.rend()-1);
//                upperBoundpath.insert(upperBoundpath.end(), outGoingPath.begin(), outGoingPath.end());
            }
        }
    }
    upperBoundTimer = Utils::getTimer() - upperBoundTimer;
    logRow.append(to_string(upperBound));
    logRow.append(to_string(upperBoundTimer));
    
    // update global upperbound
    temp.distance = upperBound;
    upperBoundForShortestPath = temp;

    ShortestPath sp =getShortestPath3(source, destination);

    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp);

    // reset upperBound and get the time

    resetFinalValues();
    totalExecutionTime = Utils::getTimer();
    ShortestPath sp2 =getShortestPath3(source, destination);
    if (sp.distance != sp2.distance) {
        cout << "ERROR\n" <<endl;
    }
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp2.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp2);
    
//    resetFinalValues();
//    totalExecutionTime = Utils::getTimer();
//    ShortestPath sp3 =getShortestPath_serial(source, destination);
//    if (sp.distance != sp3.distance) {
//        cout << "ERROR\n" <<endl;
//    }
//    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
//    logRow.append(to_string(sp3.distance));
//    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp);
    
    Utils::logCSVRow(DEBUG_LOG, logRow);
    
    return sp2;
}


ShortestPath GraphWithShortestPath::getShortestPath_serial_all(uint32_t source, uint32_t destination){
    CSVRow logRow;
    logRow.append("MAIN");
    // reset upperBound and get the time
    
    resetFinalValues();
    uint64_t totalExecutionTime = Utils::getTimer();
    totalExecutionTime = Utils::getTimer();
    ShortestPath sp3 =getShortestPath_serial(source, destination);
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp3.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp3);
    
    resetFinalValues();
    totalExecutionTime = Utils::getTimer();
    ShortestPath sp4 =getShortestPath_serial_bidirectional_fifo(source, destination);
    if (sp4.distance != sp3.distance) {
        cout << "ERROR\n" <<endl;
    }
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp4.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp4);
    
    resetFinalValues();
    totalExecutionTime = Utils::getTimer();
    ShortestPath sp5 =getShortestPath_serial_bidirectional_pq(source, destination);
    if (sp5.distance != sp3.distance) {
        cout << "ERROR\n" <<endl;
    }
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp5.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp5);
    
    resetFinalValues();
    totalExecutionTime = Utils::getTimer();
    ShortestPath sp6 =getShortestPath3(source, destination);
    if (sp6.distance != sp3.distance) {
        cout << "ERROR\n" <<endl;
    }
    totalExecutionTime = Utils::getTimer() - totalExecutionTime;
    logRow.append(to_string(sp6.distance));
    logRow.append(to_string(totalExecutionTime));
    writeShortestPathToOutput(source, destination, sp6);

    Utils::logCSVRow(DEBUG_LOG, logRow);
    
    return sp3;
}

ShortestPath GraphWithShortestPath::getShortestPath_serial_bidirectional_pq(uint32_t sourceIndex, uint32_t destinationIndex){
    ShortestPath toReturn;
    toReturn.distance = numeric_limits<uint32_t>::max();
    resizeVectors(allVerticesVector.size());
    resetAllValues();
    
    CSVRow logRow;
    logRow.append("PARENT_SERIAL_BI");
    
    globalNumberOfVerticesToSchedule++;
    globalNumberOfVerticesScheduled++;
    allVertices[sourceIndex].setDistanceToVertexFromSource(0);
    allVertices[sourceIndex].predecessorFromSource = numeric_limits<uint32_t>::max();
    
    allVertices[destinationIndex].setDistanceFromVertexToDestination(0);
    allVertices[destinationIndex].predecessorFromSource = numeric_limits<uint32_t>::max();

    priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair> > distancesPriorityQueue;
//    priority_queue<pqDistancePair, vector<pqDistancePair>, greater<pqDistancePair> > distancesPriorityQueue;
    
    distancesPriorityQueue.push(make_pair(0, sourceIndex));
    distancesPriorityQueue.push(make_pair(0, destinationIndex));
//    distancesPriorityQueue.push(make_pair(0, make_pair(false, sourceIndex)));
//    distancesPriorityQueue.push(make_pair(0, make_pair(true, destinationIndex)));
    while (!distancesPriorityQueue.empty()) {
//        bool fromDestination = distancesPriorityQueue.top().second.first;
//        uint32_t currentVertexIndex = distancesPriorityQueue.top().second.second;
        uint32_t currentVertexIndex = distancesPriorityQueue.top().second;
        uint32_t currentVertexDistance = distancesPriorityQueue.top().first;
        UpperBound tempUpperBound = upperBoundForShortestPath;
        if (currentVertexDistance >= tempUpperBound.distance) {
            // Any distance calculated after this is bound to be greater than the upperBound. So, we stop here.
            toReturn.distance = tempUpperBound.distance;
            toReturn.joiningVertex = tempUpperBound.joiningVertex;
            break;
        }
        globalNumberOfVerticesComputed++;
        distancesPriorityQueue.pop();
        
        uint32_t currentVertexDistanceFromSource = allVertices[currentVertexIndex].distanceToVertexFromSource;
        bool processOutEdges = allVertices[currentVertexIndex].isVisitedFromSource();
        bool processInEdges = allVertices[currentVertexIndex].isVisitedFromDestination();
        allVertices[currentVertexIndex].resetFlags();
        // OutEdges
        if (processOutEdges &&(currentVertexDistanceFromSource < tempUpperBound.distance)) {
            for (GraphEdge edge : allVerticesVector[currentVertexIndex].outEdges) {
                uint32_t weight = edge.getWeight();
                uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
                bool neighbouringVertexVisitedFromDestinationFlag = allVertices[neighbouringVertexIndex].isVisitedFromDestination();
                uint32_t currentDistanceToNeighbouringVertex = allVertices[currentVertexIndex].distanceToVertexFromSource + (uint32_t)weight;
                
                if (neighbouringVertexVisitedFromDestinationFlag) {
                    // update upperBound and path if needed
                    tempUpperBound = upperBoundForShortestPath;
                    uint32_t finalDistance = allVertices[currentVertexIndex].distanceToVertexFromSource + allVertices[neighbouringVertexIndex].distanceFromVertexToDestination + weight;
                    if(finalDistance < tempUpperBound.distance){
                        tempUpperBound.distance = finalDistance;
                        tempUpperBound.joiningVertex = neighbouringVertexIndex;
                        upperBoundForShortestPath = tempUpperBound;
                    }
                }
                
                if (allVertices[neighbouringVertexIndex].distanceToVertexFromSource > currentDistanceToNeighbouringVertex) {
                    // update the distance and push the vertex to priority queue
                    allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(currentDistanceToNeighbouringVertex);
                    allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
                    globalNumberOfVerticesToSchedule++;
                    if (allVertices[neighbouringVertexIndex].distanceToVertexFromSource < tempUpperBound.distance) {
                        globalNumberOfVerticesScheduled++;
                        distancesPriorityQueue.push(make_pair(currentDistanceToNeighbouringVertex, neighbouringVertexIndex));
                    }
                }
            }
        }
        
        uint32_t currentVertexDistanceaToDestination = allVertices[currentVertexIndex].distanceFromVertexToDestination;
        if (processInEdges &&(currentVertexDistanceaToDestination < tempUpperBound.distance)) {
            for (GraphEdge edge : allVerticesVector[currentVertexIndex].inEdges) {
                uint32_t weight = edge.getWeight();
                uint32_t neighbouringVertexIndex = edge.getSourceVertex();
                bool neighbouringVertexVisitedFromSourceFlag = allVertices[neighbouringVertexIndex].isVisitedFromSource();
                uint32_t currentDistanceToNeighbouringVertex = allVertices[currentVertexIndex].distanceFromVertexToDestination + (uint32_t)weight;
               
                if (neighbouringVertexVisitedFromSourceFlag) {
                    // update upperBound and path if needed
                    tempUpperBound = upperBoundForShortestPath;
                    uint32_t finalDistance = allVertices[currentVertexIndex].distanceFromVertexToDestination + allVertices[neighbouringVertexIndex].distanceToVertexFromSource + weight;
                    if(finalDistance < tempUpperBound.distance){
                        tempUpperBound.distance = finalDistance;
                        tempUpperBound.joiningVertex = neighbouringVertexIndex;
                        upperBoundForShortestPath = tempUpperBound;
                    }
                }

                if (allVertices[neighbouringVertexIndex].distanceFromVertexToDestination > currentDistanceToNeighbouringVertex) {
                    // update the distance and push the vertex to priority queue
                    allVertices[neighbouringVertexIndex].setDistanceFromVertexToDestination(currentDistanceToNeighbouringVertex);
                    allVertices[neighbouringVertexIndex].predecessorFromDestination = currentVertexIndex;
                    globalNumberOfVerticesToSchedule++;
                    if (allVertices[neighbouringVertexIndex].distanceFromVertexToDestination < tempUpperBound.distance) {
                        globalNumberOfVerticesScheduled++;
                        distancesPriorityQueue.push(make_pair(currentDistanceToNeighbouringVertex, neighbouringVertexIndex));
                    }
                }
            }

        }
        
    }
    
    logRow.append(to_string(globalNumberOfVerticesToSchedule));
    logRow.append(to_string(globalNumberOfVerticesScheduled));
    logRow.append(to_string(globalNumberOfVerticesComputed));
    Utils::logCSVRow(DEBUG_LOG, logRow);

    return toReturn;
}


void evaluateNeighbours_unidirectional_fifo(uint32_t currentVertexIndex, GraphWithShortestPath *myGraph, Logger *debugLogger, vector<TimerAggregator> &timerAggregators, UpperBound &localUpperBoundForShortestPath, uint64_t &localNumberOfVerticesComputed, uint64_t &localNumberOfVerticesToSchedule, uint64_t &localNumberOfVerticesScheduled, uint32_t &localUpperBoundUpdateLastUpdatedCount){
    
    
    uint32_t currentVertexDistanceFromSource;
    TimerAggregator &upperBoundRLockTimerAggregator = timerAggregators[0];
    TimerAggregator &upperBoundWLockTimerAggregator = timerAggregators[1];
    TimerAggregator &scheduleVertexTimerAggregator = timerAggregators[2];
    uint64_t upperBoundRLockTimer;
    
    

    // cant lock the vertex for the entire duration as it will create deadlock
    readLockVertex(currentVertexIndex);
    currentVertexDistanceFromSource = allVertices[currentVertexIndex].distanceToVertexFromSource;
    unlockVertex(currentVertexIndex);
    
    // NOTE : We need not check for visitedFromSource flag as in unidirectional we always visit from source.
    bool shouldEvaluateOutEdges = false;
    
    if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
        upperBoundRLockTimer = Utils::getDebugTimer();
        localUpperBoundForShortestPath = upperBoundForShortestPath;
        upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
        upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
        localUpperBoundUpdateLastUpdatedCount = 0;
    }
    
    if (currentVertexDistanceFromSource < localUpperBoundForShortestPath.distance) {
        shouldEvaluateOutEdges = true;
    }
    
    if (!shouldEvaluateOutEdges) {
        return;
    }
    
    localNumberOfVerticesComputed++;
    
    GraphVertex *currentVertex;
    vector<GraphEdge> inEdges, outEdges;
    
    readLockVertex(currentVertexIndex);
    currentVertex = &myGraph->allVerticesVector[currentVertexIndex];
    if (shouldEvaluateOutEdges) {
        outEdges = currentVertex->outEdges;
    }
    unlockVertex(currentVertexIndex);
    
    upperBoundRLockTimer = Utils::getDebugTimer();
    
    for (GraphEdge &edge : outEdges) {
        uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
        
        uint64_t upperBoundWLockTimer, scheduleVertexTimer;
        
        // lock and get the neighbouring Vertex values;
        writeLockVertex(neighbouringVertexIndex);
        bool neighbouringVertexVisitedFromDestinationFlag = allVertices[neighbouringVertexIndex].isVisitedFromDestination();
        uint32_t neighbouringVertexDistanceFromSource = allVertices[neighbouringVertexIndex].distanceToVertexFromSource;
        uint32_t distanceToReachNeighbouringVertexThroughCurrentVertex = currentVertexDistanceFromSource + (uint32_t)edge.weight;
        
        if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
            upperBoundRLockTimer = Utils::getDebugTimer();
            localUpperBoundForShortestPath = upperBoundForShortestPath;
            upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
            upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
            localUpperBoundUpdateLastUpdatedCount = 0;
        }
        
//        if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceFromSource) {
//            allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(distanceToReachNeighbouringVertexThroughCurrentVertex);
//            allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
//        }
        
        if (distanceToReachNeighbouringVertexThroughCurrentVertex > localUpperBoundForShortestPath.distance) {
            // We can just quit here as any path through neighBouringVertex is going to be of greater distance than upperBound
            unlockVertex(neighbouringVertexIndex);
            continue;
        }
        
        if (neighbouringVertexVisitedFromDestinationFlag) {
            // this means that the neighbouring vertex is the destination vertex.
            upperBoundWLockTimer = Utils::getDebugTimer();
            
            UpperBound currentValue;
            currentValue.distance = distanceToReachNeighbouringVertexThroughCurrentVertex;
            currentValue.joiningVertex = neighbouringVertexIndex;
            localUpperBoundForShortestPath = currentValue;
            localUpperBoundUpdateLastUpdatedCount = 0;
            UpperBound prev_value = upperBoundForShortestPath;
            while (true) {
                if (currentValue.distance > prev_value.distance) {
                    upperBoundRLockTimer = Utils::getDebugTimer();
                    localUpperBoundForShortestPath = upperBoundForShortestPath;
                    upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                    upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                    break;
                }
                if (upperBoundForShortestPath.compare_exchange_weak(prev_value, currentValue)) {
                    break;
                }
            }
            upperBoundWLockTimer = Utils::getDebugTimer() - upperBoundWLockTimer;
            upperBoundWLockTimerAggregator.add(upperBoundWLockTimer);
        }
        
        
        
        // Update the path taken to reach the neighbouringVertex if the distance of the path through the currentVertex is lesser than the distance of the already computed path of the neighbouringVertex
        if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceFromSource) {
            
            allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(distanceToReachNeighbouringVertexThroughCurrentVertex);
            allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
            
            localNumberOfVerticesToSchedule++;
            if (distanceToReachNeighbouringVertexThroughCurrentVertex < localUpperBoundForShortestPath.distance) {
                scheduleVertexTimer = Utils::getDebugTimer();
                if(scheduleVertex(neighbouringVertexIndex))
                    localNumberOfVerticesScheduled++;
                scheduleVertexTimer = Utils::getDebugTimer() - scheduleVertexTimer;
                scheduleVertexTimerAggregator.add(scheduleVertexTimer);
            }
        }
        
        unlockVertex(neighbouringVertexIndex);
    }
}

void shortestPathBetween2VerticesComputeFunction_fifo(unsigned threadId, void* arguments){
    // Function with threadPool
    struct argumentStructureForShortestPathComputeFunction *argumentStructure = (struct argumentStructureForShortestPathComputeFunction*) arguments;
    GraphWithShortestPath *myGraph = argumentStructure->myGraph;
    uint32_t sourceIndex = argumentStructure->sourceIndex;
    uint32_t destinationIndex = argumentStructure->destinationIndex;
    string queryString = to_string(sourceIndex);
    queryString = queryString + "->" + to_string(destinationIndex);
    
    CSVRow logRow, logRow1, logRow2, logRow3, logRow4;
    logRow3.append(queryString);
    
    uint32_t currentVertexIndex;
    uint64_t localNumberOfVerticesScheduled = 0;
    uint64_t localNumberOfVerticesToSchedule = 0;
    uint64_t localNumberOfVerticesComputed = 0;
    
    Logger debugLogger;
    debugLogger.attachLogFile(Utils::getOutputFolderPath() + to_string(threadId) + "_threadLog.csv");
    debugLogger.setShouldPrintToConsole(false);
    
    uint64_t nextVertexTimer, totalThreadTime=Utils::getDebugTimer(), waitingForNextVertexTimer;
    
    TimerAggregator nextVertexTimerAggregator, waitingForNextVertexTimerAggregator;
    vector<TimerAggregator> timerAggregators(3);
    
    UpperBound localUpperBoundForShortestPath = upperBoundForShortestPath , prevUpperBound = upperBoundForShortestPath;
    uint32_t localUpperBoundUpdateLastUpdatedCount = 0;
    //    uint32_t localUpperBoundForShortestPath = numeric_limits<uint32_t>::max();
    bool nextVertexIsAvailable = false;
    uint32_t count = 0;
    uint64_t timer = Utils::getTimer();

    while(true){
        
        nextVertexTimer = Utils::getDebugTimer();
        nextVertexIsAvailable = getNextVertexToProcess(currentVertexIndex);
        
        nextVertexTimer = Utils::getDebugTimer() - nextVertexTimer;
        nextVertexTimerAggregator.add(nextVertexTimer);
        if(!nextVertexIsAvailable){
            
            if (hasCompletedProcessing()) {
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                totalThreadTime = Utils::getDebugTimer() - totalThreadTime;
                
                globalNumberOfVerticesToSchedule += localNumberOfVerticesToSchedule;
                globalNumberOfVerticesScheduled += localNumberOfVerticesScheduled;
                globalNumberOfVerticesComputed += localNumberOfVerticesComputed;
                
                logRow3.append(to_string(localNumberOfVerticesToSchedule));
                logRow3.append(to_string(localNumberOfVerticesScheduled));
                logRow3.append(to_string(localNumberOfVerticesComputed));

//                debugLogger.writeLog(queryString);
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                debugLogger.writeLog(logRow3.getString());
                debugLogger.writeLog(logRow4.getString());
                debugLogger.writeLog(logRow2.getString());
                debugLogger.writeLog(logRow1.getString());
                

                
                //                if (usePriorityQueue) {
                //                    logRow.append("PQ");
                //                }
                //                else{
                //                    logRow.append("FIFO");
                //                }
                //                logRow.append("FINAL");
                //                logRow.append(to_string(totalThreadTime));
                //                logRow.append(nextVertexTimerAggregator.totalValue());
                //                logRow.append(waitingForNextVertexTimerAggregator.totalValue());
                //                for (unsigned i=0; i<timerAggregators.size(); i++) {
                //                    logRow.append(timerAggregators[i].totalValue());
                //                }
                //                debugLogger.writeLog(logRow.getString());
                //                logRow.clear();
                //
                //
                //                if (usePriorityQueue) {
                //                    logRow.append("PQ");
                //                }
                //                else{
                //                    logRow.append("FIFO");
                //                }
                //                logRow.append("FINAL - detailed");
                //                logRow.append(to_string(totalThreadTime));
                //                logRow.append(nextVertexTimerAggregator.finalValue());
                //                logRow.append(waitingForNextVertexTimerAggregator.finalValue());
                //                for (unsigned i=0; i<timerAggregators.size(); i++) {
                //                    logRow.append(timerAggregators[i].finalValue());
                //                }
                //                debugLogger.writeLog(logRow.getString());
                return;
                // sync threadPool
                // the thread will now start waiting at this barrier point
                //                continue;
            }
            
            // wait till new vertex is added
            pthread_mutex_lock(&waitingForNextVertex.mutex);
            waitingForNextVertex.counter++;
            if (waitingForNextVertex.counter == Utils::numberOfThreads) {
                
                waitingForNextVertex.counter--;
                
                // Update the completedProcessing flag
                lockForCompletedProcessing.writeLock();
                completedProcessing = true;
                lockForCompletedProcessing.unlock();
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                
                // We will also signal all threads waiting for the next vertex
                pthread_cond_broadcast(&waitingForNextVertex.conditional);
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                
            }
            else{
                struct timespec max_wait = Utils::getTimeSpec(2);
                waitingForNextVertexTimer = Utils::getDebugTimer();
                pthread_cond_timedwait(&waitingForNextVertex.conditional, &waitingForNextVertex.mutex, &max_wait);
                waitingForNextVertex.counter--;
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                waitingForNextVertexTimer = Utils::getDebugTimer() - waitingForNextVertexTimer;
                waitingForNextVertexTimerAggregator.add(waitingForNextVertexTimer);
            }
            continue;
        }
        
        
        
        count++;
//        if (count%1000) {
//            logRow.append(to_string(localUpperBoundForShortestPath.distance));
//            logRow1.append(to_string(Utils::getTimer() - timer));
            if (prevUpperBound.distance != localUpperBoundForShortestPath.distance) {
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                prevUpperBound = localUpperBoundForShortestPath;
            }
//        }

        evaluateNeighbours_unidirectional_fifo(currentVertexIndex, myGraph, &debugLogger, timerAggregators, localUpperBoundForShortestPath, localNumberOfVerticesComputed,  localNumberOfVerticesToSchedule, localNumberOfVerticesScheduled, localUpperBoundUpdateLastUpdatedCount);
        
    }
    return;
}


void evaluateNeighbours_unidirectional_pq(uint32_t currentVertexIndex, GraphWithShortestPath *myGraph, Logger *debugLogger, vector<TimerAggregator> &timerAggregators, UpperBound &localUpperBoundForShortestPath, uint64_t &localNumberOfVerticesComputed, uint64_t &localNumberOfVerticesToSchedule, uint64_t &localNumberOfVerticesScheduled, uint32_t &localUpperBoundUpdateLastUpdatedCount){

    uint32_t currentVertexDistanceFromSource;
    TimerAggregator &upperBoundRLockTimerAggregator = timerAggregators[0];
    TimerAggregator &upperBoundWLockTimerAggregator = timerAggregators[1];
    TimerAggregator &scheduleVertexTimerAggregator = timerAggregators[2];
    uint64_t upperBoundRLockTimer;
    
    
    // cant lock the vertex for the entire duration as it will create deadlock
    readLockVertex(currentVertexIndex);
    currentVertexDistanceFromSource = allVertices[currentVertexIndex].distanceToVertexFromSource;
    unlockVertex(currentVertexIndex);
    
    bool shouldEvaluateOutEdges = false;
    
    if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
        upperBoundRLockTimer = Utils::getDebugTimer();
        localUpperBoundForShortestPath = upperBoundForShortestPath;
        upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
        upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
        localUpperBoundUpdateLastUpdatedCount = 0;
    }
    
    if (currentVertexDistanceFromSource < localUpperBoundForShortestPath.distance) {
        shouldEvaluateOutEdges = true;
    }
    
    if (!shouldEvaluateOutEdges) {
        return;
    }
    
    localNumberOfVerticesComputed++;
    
    GraphVertex *currentVertex;
    vector<GraphEdge> inEdges, outEdges;
    
    readLockVertex(currentVertexIndex);
    currentVertex = &myGraph->allVerticesVector[currentVertexIndex];
    if (shouldEvaluateOutEdges) {
        outEdges = currentVertex->outEdges;
    }
    unlockVertex(currentVertexIndex);
    
    upperBoundRLockTimer = Utils::getDebugTimer();
    
    for (GraphEdge &edge : outEdges) {
        uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
        
        uint64_t upperBoundWLockTimer, scheduleVertexTimer;
        
        // lock and get the neighbouring Vertex values;
        writeLockVertex(neighbouringVertexIndex);
        bool neighbouringVertexVisitedFromDestinationFlag = allVertices[neighbouringVertexIndex].isVisitedFromDestination();
        uint32_t neighbouringVertexDistanceFromSource = allVertices[neighbouringVertexIndex].distanceToVertexFromSource;
        uint32_t distanceToReachNeighbouringVertexThroughCurrentVertex = currentVertexDistanceFromSource + (uint32_t)edge.weight;
        
        if (++localUpperBoundUpdateLastUpdatedCount % LOCAL_UPPER_BOUND_UPDATE_INTERVAL == 0) {
            upperBoundRLockTimer = Utils::getDebugTimer();
            localUpperBoundForShortestPath = upperBoundForShortestPath;
            upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
            upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
            localUpperBoundUpdateLastUpdatedCount = 0;
        }
        
        //        if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceFromSource) {
        //            allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(distanceToReachNeighbouringVertexThroughCurrentVertex);
        //            allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
        //        }
        
        if (distanceToReachNeighbouringVertexThroughCurrentVertex > localUpperBoundForShortestPath.distance) {
            // We can just quit here as any path through neighBouringVertex is going to be of greater distance than upperBound
            unlockVertex(neighbouringVertexIndex);
            continue;
        }
        
        if (neighbouringVertexVisitedFromDestinationFlag) {
            // this means that the neighbouring vertex is the destination vertex.
            upperBoundWLockTimer = Utils::getDebugTimer();
            
            UpperBound currentValue;
            currentValue.distance = distanceToReachNeighbouringVertexThroughCurrentVertex;
            currentValue.joiningVertex = neighbouringVertexIndex;
            localUpperBoundForShortestPath = currentValue;
            localUpperBoundUpdateLastUpdatedCount = 0;
            UpperBound prev_value = upperBoundForShortestPath;
            while (true) {
                if (currentValue.distance > prev_value.distance) {
                    upperBoundRLockTimer = Utils::getDebugTimer();
                    localUpperBoundForShortestPath = upperBoundForShortestPath;
                    upperBoundRLockTimer = Utils::getDebugTimer() - upperBoundRLockTimer;
                    upperBoundRLockTimerAggregator.add(upperBoundRLockTimer);
                    break;
                }
                if (upperBoundForShortestPath.compare_exchange_weak(prev_value, currentValue)) {
                    break;
                }
            }
            upperBoundWLockTimer = Utils::getDebugTimer() - upperBoundWLockTimer;
            upperBoundWLockTimerAggregator.add(upperBoundWLockTimer);
        }
        
        
        
        // Update the path taken to reach the neighbouringVertex if the distance of the path through the currentVertex is lesser than the distance of the already computed path of the neighbouringVertex
        if (distanceToReachNeighbouringVertexThroughCurrentVertex < neighbouringVertexDistanceFromSource) {
            
            allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(distanceToReachNeighbouringVertexThroughCurrentVertex);
            allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
            
            localNumberOfVerticesToSchedule++;
            if (distanceToReachNeighbouringVertexThroughCurrentVertex < localUpperBoundForShortestPath.distance) {
                scheduleVertexTimer = Utils::getDebugTimer();
                if(scheduleVertex_pq(neighbouringVertexIndex, distanceToReachNeighbouringVertexThroughCurrentVertex))
                    localNumberOfVerticesScheduled++;
                scheduleVertexTimer = Utils::getDebugTimer() - scheduleVertexTimer;
                scheduleVertexTimerAggregator.add(scheduleVertexTimer);
            }
        }
        
        unlockVertex(neighbouringVertexIndex);
    }
}

void shortestPathBetween2VerticesComputeFunction_pq(unsigned threadId, void* arguments){
    // Function with threadPool
    struct argumentStructureForShortestPathComputeFunction *argumentStructure = (struct argumentStructureForShortestPathComputeFunction*) arguments;
    GraphWithShortestPath *myGraph = argumentStructure->myGraph;
    uint32_t sourceIndex = argumentStructure->sourceIndex;
    uint32_t destinationIndex = argumentStructure->destinationIndex;
    string queryString = to_string(sourceIndex);
    queryString = queryString + "->" + to_string(destinationIndex);
    
    CSVRow logRow, logRow1, logRow2, logRow3, logRow4;
    logRow3.append(queryString);
    
    uint32_t currentVertexIndex;
    uint64_t localNumberOfVerticesScheduled = 0;
    uint64_t localNumberOfVerticesToSchedule = 0;
    uint64_t localNumberOfVerticesComputed = 0;
    
    Logger debugLogger;
    debugLogger.attachLogFile(Utils::getOutputFolderPath() + to_string(threadId) + "_threadLog.csv");
    debugLogger.setShouldPrintToConsole(false);
    
    uint64_t nextVertexTimer, totalThreadTime=Utils::getDebugTimer(), waitingForNextVertexTimer;
    
    TimerAggregator nextVertexTimerAggregator, waitingForNextVertexTimerAggregator;
    vector<TimerAggregator> timerAggregators(3);
    
    UpperBound localUpperBoundForShortestPath = upperBoundForShortestPath , prevUpperBound = upperBoundForShortestPath;
    uint32_t localUpperBoundUpdateLastUpdatedCount = 0;
    //    uint32_t localUpperBoundForShortestPath = numeric_limits<uint32_t>::max();
    bool nextVertexIsAvailable = false;
    uint64_t count = 0;
    uint64_t timer = Utils::getTimer();

    while(true){
        
        nextVertexTimer = Utils::getDebugTimer();
        nextVertexIsAvailable = getNextVertexToProcess_pq(currentVertexIndex, localUpperBoundForShortestPath.distance);
        
        nextVertexTimer = Utils::getDebugTimer() - nextVertexTimer;
        nextVertexTimerAggregator.add(nextVertexTimer);
        if(!nextVertexIsAvailable){
            
            if (hasCompletedProcessing()) {
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                totalThreadTime = Utils::getDebugTimer() - totalThreadTime;
                
                globalNumberOfVerticesToSchedule += localNumberOfVerticesToSchedule;
                globalNumberOfVerticesScheduled += localNumberOfVerticesScheduled;
                globalNumberOfVerticesComputed += localNumberOfVerticesComputed;
                
                logRow3.append(to_string(localNumberOfVerticesToSchedule));
                logRow3.append(to_string(localNumberOfVerticesScheduled));
                logRow3.append(to_string(localNumberOfVerticesComputed));

//                debugLogger.writeLog(queryString);
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                debugLogger.writeLog(logRow3.getString());
                debugLogger.writeLog(logRow4.getString());
                debugLogger.writeLog(logRow2.getString());
                debugLogger.writeLog(logRow1.getString());
                


                //                if (usePriorityQueue) {
                //                    logRow.append("PQ");
                //                }
                //                else{
                //                    logRow.append("FIFO");
                //                }
                //                logRow.append("FINAL");
                //                logRow.append(to_string(totalThreadTime));
                //                logRow.append(nextVertexTimerAggregator.totalValue());
                //                logRow.append(waitingForNextVertexTimerAggregator.totalValue());
                //                for (unsigned i=0; i<timerAggregators.size(); i++) {
                //                    logRow.append(timerAggregators[i].totalValue());
                //                }
                //                debugLogger.writeLog(logRow.getString());
                //                logRow.clear();
                //
                //
                //                if (usePriorityQueue) {
                //                    logRow.append("PQ");
                //                }
                //                else{
                //                    logRow.append("FIFO");
                //                }
                //                logRow.append("FINAL - detailed");
                //                logRow.append(to_string(totalThreadTime));
                //                logRow.append(nextVertexTimerAggregator.finalValue());
                //                logRow.append(waitingForNextVertexTimerAggregator.finalValue());
                //                for (unsigned i=0; i<timerAggregators.size(); i++) {
                //                    logRow.append(timerAggregators[i].finalValue());
                //                }
                //                debugLogger.writeLog(logRow.getString());
                return;
                // sync threadPool
                // the thread will now start waiting at this barrier point
                //                continue;
            }
            
            // wait till new vertex is added
            pthread_mutex_lock(&waitingForNextVertex.mutex);
            waitingForNextVertex.counter++;
            if (waitingForNextVertex.counter == Utils::numberOfThreads) {
                
                waitingForNextVertex.counter--;
                
                // Update the completedProcessing flag
                lockForCompletedProcessing.writeLock();
                completedProcessing = true;
                lockForCompletedProcessing.unlock();
                
                pthread_mutex_lock(&completedProcessingConditional.mutex);
                pthread_cond_signal(&completedProcessingConditional.conditional);
                pthread_mutex_unlock(&completedProcessingConditional.mutex);
                
                // We will also signal all threads waiting for the next vertex
                pthread_cond_broadcast(&waitingForNextVertex.conditional);
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                
            }
            else{
                struct timespec max_wait = Utils::getTimeSpec(2);
                waitingForNextVertexTimer = Utils::getDebugTimer();
                pthread_cond_timedwait(&waitingForNextVertex.conditional, &waitingForNextVertex.mutex, &max_wait);
                waitingForNextVertex.counter--;
                pthread_mutex_unlock(&waitingForNextVertex.mutex);
                waitingForNextVertexTimer = Utils::getDebugTimer() - waitingForNextVertexTimer;
                waitingForNextVertexTimerAggregator.add(waitingForNextVertexTimer);
            }
            continue;
        }
        
        
        
        count++;
//        if (count%1000) {
//            logRow.append(to_string(localUpperBoundForShortestPath.distance));
        
            if (prevUpperBound.distance != localUpperBoundForShortestPath.distance) {
                logRow2.append(to_string(count));
                logRow4.append(to_string(localUpperBoundForShortestPath.distance));
                logRow1.append(to_string(Utils::getTimer() - timer));
                prevUpperBound = localUpperBoundForShortestPath;
            }
//        }

        evaluateNeighbours_unidirectional_pq(currentVertexIndex, myGraph, &debugLogger, timerAggregators, localUpperBoundForShortestPath, localNumberOfVerticesComputed,  localNumberOfVerticesToSchedule, localNumberOfVerticesScheduled, localUpperBoundUpdateLastUpdatedCount);
        
    }
    return;
}


ShortestPath GraphWithShortestPath::getShortestPath_parallel_unidirectional(uint32_t sourceIndex, uint32_t destinationIndex, bool usePriorityQueue){
    
    ShortestPath toReturn;
    toReturn.distance = numeric_limits<uint32_t>::max();
    resetAllValues();
    
    allVertices[sourceIndex].setDistanceToVertexFromSource(0);
    allVertices[destinationIndex].setDistanceFromVertexToDestination(0);
    if(usePriorityQueue){
        scheduleVertex_pq(sourceIndex, 0);
    }
    else{
        scheduleVertex(sourceIndex);
    }
    
    // update threadInfo arguments
    for (unsigned i=0; i<Utils::getNumberOfThreads(); i++) {
        struct argumentStructureForShortestPathComputeFunction *arg = (struct argumentStructureForShortestPathComputeFunction*)threadPool.getThreadInfoArgument(i);
        arg->usePriorityQueue = usePriorityQueue;
        arg->sourceIndex = sourceIndex;
        arg->destinationIndex = destinationIndex;
        threadPool.setThreadInfoArgument(i, arg);
    }
    
    globalNumberOfVerticesScheduled++;
    globalNumberOfVerticesToSchedule++;

    if (usePriorityQueue) {
        threadPool.perform(&shortestPathBetween2VerticesComputeFunction_pq);
    }
    else{
        threadPool.perform(&shortestPathBetween2VerticesComputeFunction_fifo);
    }
    
    
    lockForCompletedProcessing.readLock();
    if (completedProcessing == false) {
        pthread_mutex_lock(&completedProcessingConditional.mutex);
        lockForCompletedProcessing.unlock();
        pthread_cond_wait(&completedProcessingConditional.conditional, &completedProcessingConditional.mutex);
        pthread_mutex_unlock(&completedProcessingConditional.mutex);
    }
    else{
        lockForCompletedProcessing.unlock();
    }
    
    threadPool.sync();

    UpperBound tempUpperBound = upperBoundForShortestPath;
    toReturn.distance = tempUpperBound.distance;
    toReturn.joiningVertex = tempUpperBound.joiningVertex;
    CSVRow logRow;
    logRow.append("PARENT_UNIDIR");

    logRow.append(to_string(globalNumberOfVerticesToSchedule));
    logRow.append(to_string(globalNumberOfVerticesScheduled));
    logRow.append(to_string(globalNumberOfVerticesComputed));

    Utils::logCSVRow(DEBUG_LOG, logRow);


    return toReturn;

}

ShortestPath GraphWithShortestPath::getShortestPath_parallel_unidirectional(uint32_t source, uint32_t destination){
    return getShortestPath_parallel_unidirectional(source, destination, false);
}


ShortestPath GraphWithShortestPath::getShortestPath_serial_bidirectional(uint32_t sourceIndex, uint32_t destinationIndex, bool usePriorityQueue){
    if (usePriorityQueue) {
        return getShortestPath_serial_bidirectional_pq(sourceIndex, destinationIndex);
    }
    else{
        return getShortestPath_serial_bidirectional_fifo(sourceIndex, destinationIndex);
    }
}
    
ShortestPath GraphWithShortestPath::getShortestPath_serial_bidirectional_fifo(uint32_t sourceIndex, uint32_t destinationIndex){
    ShortestPath toReturn;
    toReturn.distance = numeric_limits<uint32_t>::max();
    resizeVectors(allVerticesVector.size());
    resetAllValues();
    
    CSVRow logRow;
    logRow.append("PARENT_SERIAL_BI");
    
    globalNumberOfVerticesToSchedule++;
    globalNumberOfVerticesScheduled++;
    allVertices[sourceIndex].setDistanceToVertexFromSource(0);
    allVertices[sourceIndex].predecessorFromSource = numeric_limits<uint32_t>::max();
    
    allVertices[destinationIndex].setDistanceFromVertexToDestination(0);
    allVertices[destinationIndex].predecessorFromSource = numeric_limits<uint32_t>::max();

    nextVertexQueue.push(sourceIndex);
    nextVertexQueue.push(destinationIndex);
    
    while (!nextVertexQueue.empty()) {
        //        bool fromDestination = distancesPriorityQueue.top().second.first;
        //        uint32_t currentVertexIndex = distancesPriorityQueue.top().second.second;
        uint32_t currentVertexIndex = nextVertexQueue.front();
        nextVertexQueue.pop();
        isNextVertexScheduled.unschedule(currentVertexIndex, true);
        uint32_t currentVertexDistanceFromSource = allVertices[currentVertexIndex].distanceToVertexFromSource;
        uint32_t currentVertexDistanceToDestination = allVertices[currentVertexIndex].distanceFromVertexToDestination;

        bool shouldEvaluateOutEdges = allVertices[currentVertexIndex].isVisitedFromSource();
        bool shouldEvaluateInEdges = allVertices[currentVertexIndex].isVisitedFromDestination();
        allVertices[currentVertexIndex].resetFlags();
        
        UpperBound tempUpperBound = upperBoundForShortestPath;
        if (shouldEvaluateOutEdges && (currentVertexDistanceFromSource < tempUpperBound.distance)) {
            shouldEvaluateOutEdges = true;
        }
        if (shouldEvaluateInEdges && (currentVertexDistanceToDestination < tempUpperBound.distance)) {
            shouldEvaluateInEdges = true;
        }

        if (shouldEvaluateInEdges || shouldEvaluateOutEdges) {
            globalNumberOfVerticesComputed++;
        }
        else{
            continue;
        }
        
        // OutEdges
        if (shouldEvaluateOutEdges) {
            for (GraphEdge edge : allVerticesVector[currentVertexIndex].outEdges) {
                uint32_t weight = edge.getWeight();
                uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
                bool neighbouringVertexVisitedFromDestinationFlag = allVertices[neighbouringVertexIndex].isVisitedFromDestination();
                uint32_t currentDistanceToNeighbouringVertex = allVertices[currentVertexIndex].distanceToVertexFromSource + (uint32_t)weight;
                
                if (neighbouringVertexVisitedFromDestinationFlag) {
                    // update upperBound and path if needed
                    tempUpperBound = upperBoundForShortestPath;
                    uint32_t finalDistance = allVertices[currentVertexIndex].distanceToVertexFromSource + allVertices[neighbouringVertexIndex].distanceFromVertexToDestination + weight;
                    if(finalDistance < tempUpperBound.distance){
                        tempUpperBound.distance = finalDistance;
                        tempUpperBound.joiningVertex = neighbouringVertexIndex;
                        upperBoundForShortestPath = tempUpperBound;
                    }
                }
                
                if (allVertices[neighbouringVertexIndex].distanceToVertexFromSource > currentDistanceToNeighbouringVertex) {
                    // update the distance and push the vertex to priority queue
                    allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(currentDistanceToNeighbouringVertex);
                    allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
                    globalNumberOfVerticesToSchedule++;
                    if (allVertices[neighbouringVertexIndex].distanceToVertexFromSource < tempUpperBound.distance) {
                        if (!isNextVertexScheduled.isScheduled(neighbouringVertexIndex)) {
                            globalNumberOfVerticesScheduled++;
                            nextVertexQueue.push(neighbouringVertexIndex);
                            isNextVertexScheduled.schedule(neighbouringVertexIndex, true);
                        }
                    }
                }
            }
        }
        
        if (shouldEvaluateInEdges) {
            for (GraphEdge edge : allVerticesVector[currentVertexIndex].inEdges) {
                uint32_t weight = edge.getWeight();
                uint32_t neighbouringVertexIndex = edge.getSourceVertex();
                bool neighbouringVertexVisitedFromSourceFlag = allVertices[neighbouringVertexIndex].isVisitedFromSource();
                uint32_t currentDistanceToNeighbouringVertex = allVertices[currentVertexIndex].distanceFromVertexToDestination + (uint32_t)weight;
                
                if (neighbouringVertexVisitedFromSourceFlag) {
                    // update upperBound and path if needed
                    tempUpperBound = upperBoundForShortestPath;
                    uint32_t finalDistance = allVertices[currentVertexIndex].distanceFromVertexToDestination + allVertices[neighbouringVertexIndex].distanceToVertexFromSource + weight;
                    if(finalDistance < tempUpperBound.distance){
                        tempUpperBound.distance = finalDistance;
                        tempUpperBound.joiningVertex = neighbouringVertexIndex;
                        upperBoundForShortestPath = tempUpperBound;
                    }
                }
                
                if (allVertices[neighbouringVertexIndex].distanceFromVertexToDestination > currentDistanceToNeighbouringVertex) {
                    // update the distance and push the vertex to priority queue
                    allVertices[neighbouringVertexIndex].setDistanceFromVertexToDestination(currentDistanceToNeighbouringVertex);
                    allVertices[neighbouringVertexIndex].predecessorFromDestination = currentVertexIndex;
                    globalNumberOfVerticesToSchedule++;
                    if (allVertices[neighbouringVertexIndex].distanceFromVertexToDestination < tempUpperBound.distance) {
                        globalNumberOfVerticesScheduled++;
                        nextVertexQueue.push(neighbouringVertexIndex);
                        isNextVertexScheduled.schedule(neighbouringVertexIndex, true);
                    }
                }
            }
            
        }
        
    }
    
    logRow.append(to_string(globalNumberOfVerticesToSchedule));
    logRow.append(to_string(globalNumberOfVerticesScheduled));
    logRow.append(to_string(globalNumberOfVerticesComputed));
    Utils::logCSVRow(DEBUG_LOG, logRow);
    
    UpperBound tempUpperBound = upperBoundForShortestPath;
    toReturn.distance = tempUpperBound.distance;
    toReturn.joiningVertex = tempUpperBound.joiningVertex;
    
    return toReturn;
}

ShortestPath GraphWithShortestPath::getShortestPath_serial(uint32_t sourceIndex, uint32_t destinationIndex){
    ShortestPath toReturn;
    toReturn.distance = numeric_limits<uint32_t>::max();
    resizeVectors(allVerticesVector.size());
    resetAllValues();
    
    CSVRow logRow;
    logRow.append("PARENT_SERIAL");
    
    globalNumberOfVerticesToSchedule++;
    globalNumberOfVerticesScheduled++;
    allVertices[sourceIndex].setDistanceToVertexFromSource(0);
    allVertices[sourceIndex].predecessorFromSource = numeric_limits<uint32_t>::max();
    
    priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair> > distancesPriorityQueue;
    
    distancesPriorityQueue.push(make_pair(0, sourceIndex));
    while (!distancesPriorityQueue.empty()) {
        uint32_t currentVertexIndex = distancesPriorityQueue.top().second;
        uint32_t currentVertexDistance = distancesPriorityQueue.top().first;
        UpperBound tempUpperBound = upperBoundForShortestPath;
        if (currentVertexDistance > tempUpperBound.distance) {
            // Any distance calculated after this is bound to be greater than the upperBound. So, we stop here.
            toReturn.distance = tempUpperBound.distance;
            toReturn.joiningVertex = tempUpperBound.joiningVertex;
            break;
        }
        globalNumberOfVerticesComputed++;
        distancesPriorityQueue.pop();
        for (GraphEdge edge : allVerticesVector[currentVertexIndex].outEdges) {
            uint32_t weight = edge.getWeight();
            uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
            uint32_t currentDistanceToNeighbouringVertex = allVertices[currentVertexIndex].distanceToVertexFromSource + (uint32_t)weight;
            if (allVertices[neighbouringVertexIndex].distanceToVertexFromSource > currentDistanceToNeighbouringVertex) {
                // update the distance and push the vertex to priority queue
                allVertices[neighbouringVertexIndex].setDistanceToVertexFromSource(currentDistanceToNeighbouringVertex);
                allVertices[neighbouringVertexIndex].predecessorFromSource = currentVertexIndex;
                if (neighbouringVertexIndex == destinationIndex) {
                    // update upperBound and path if needed
                    tempUpperBound = upperBoundForShortestPath;
                    if(currentDistanceToNeighbouringVertex < tempUpperBound.distance){
                        tempUpperBound.distance = currentDistanceToNeighbouringVertex;
                        tempUpperBound.joiningVertex = neighbouringVertexIndex;
                        upperBoundForShortestPath = tempUpperBound;
                    }
                }
                globalNumberOfVerticesToSchedule++;
                globalNumberOfVerticesScheduled++;
                distancesPriorityQueue.push(make_pair(currentDistanceToNeighbouringVertex, neighbouringVertexIndex));
            }
        }
    }
    
    logRow.append(to_string(globalNumberOfVerticesToSchedule));
    logRow.append(to_string(globalNumberOfVerticesScheduled));
    logRow.append(to_string(globalNumberOfVerticesComputed));
    Utils::logCSVRow(DEBUG_LOG, logRow);
    
    return toReturn;
}

void GraphWithShortestPath::constructFullPath(uint32_t joiningVertex, vector<uint32_t> &path){
    
    path.push_back(joiningVertex);
    
    uint32_t currentVertexIndex = joiningVertex;
    // Add the path to source to the joiningVertex
    while (true) {
        currentVertexIndex = allVertices[currentVertexIndex].predecessorFromSource;
        if (currentVertexIndex == numeric_limits<uint32_t>::max()) {
            break;
        }
        path.insert(path.begin(), currentVertexIndex);
    }
    
    currentVertexIndex = joiningVertex;
    while (true) {
        currentVertexIndex = allVertices[currentVertexIndex].predecessorFromDestination;
        if (currentVertexIndex == numeric_limits<uint32_t>::max()) {
            break;
        }
        path.push_back(currentVertexIndex);
    }
    
    if (path.size() == 1) {
        path.clear();
    }
    
}


//AllShortestPaths* GraphWithShortestPath::allShortestPathsFromVertex(uint32_t sourceIndex){
//    AllShortestPaths *allPaths = new AllShortestPaths(sourceIndex, allVerticesVector.size());
//    priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair> > distancesPriorityQueue;
//
//    allPaths->allShortestPaths[sourceIndex].distance = 0;
//    allPaths->allShortestPaths[sourceIndex].path.push_back(sourceIndex);
//
//    distancesPriorityQueue.push(make_pair(0, sourceIndex));
//
//    while (!distancesPriorityQueue.empty()) {
//        uint32_t currentVertexIndex = distancesPriorityQueue.top().second;
//        uint32_t currentVertexDistance = distancesPriorityQueue.top().first;
//        distancesPriorityQueue.pop();
//        for (GraphEdge edge : allVerticesVector[currentVertexIndex].outEdges) {
//            uint32_t weight = edge.getWeight();
//            uint32_t neighbouringVertexIndex = edge.getDestinationVertex();
//            uint32_t currentDistanceToNeighbouringVertex = currentVertexDistance + weight;
//            if (allPaths->allShortestPaths[neighbouringVertexIndex].distance > currentDistanceToNeighbouringVertex) {
//                // update the distance and push the vertex to priority queue
//                allPaths->allShortestPaths[neighbouringVertexIndex].distance = currentDistanceToNeighbouringVertex;
//                vector<uint32_t> updatedPath = allPaths->allShortestPaths[currentVertexIndex].path;
//                updatedPath.push_back(neighbouringVertexIndex);
//                allPaths->allShortestPaths[neighbouringVertexIndex].path = updatedPath;
//                distancesPriorityQueue.push(make_pair(currentDistanceToNeighbouringVertex, neighbouringVertexIndex));
//            }
//        }
//    }
//    return allPaths;
//}


//AllShortestPaths* GraphWithShortestPath::allShortestPathsToVertex(uint32_t sourceIndex){
//    AllShortestPaths *allPaths = new AllShortestPaths(sourceIndex, allVerticesVector.size());
//    priority_queue<uint32_tPair, vector<uint32_tPair>, greater<uint32_tPair> > distancesPriorityQueue;
//    
//    allPaths->allShortestPaths[sourceIndex].distance = 0;
//    allPaths->allShortestPaths[sourceIndex].path.push_back(sourceIndex);
//    
//    distancesPriorityQueue.push(make_pair(0, sourceIndex));
//    
//    while (!distancesPriorityQueue.empty()) {
//        uint32_t currentVertexIndex = distancesPriorityQueue.top().second;
//        uint32_t currentVertexDistance = distancesPriorityQueue.top().first;
//        distancesPriorityQueue.pop();
//        for (GraphEdge edge : allVerticesVector[currentVertexIndex].inEdges) {
//            uint32_t weight = edge.getWeight();
//            uint32_t neighbouringVertexIndex = edge.getSourceVertex();
//            uint32_t currentDistanceToNeighbouringVertex = currentVertexDistance + weight;
//            if (allPaths->allShortestPaths[neighbouringVertexIndex].distance > currentDistanceToNeighbouringVertex) {
//                // update the distance and push the vertex to priority queue
//                allPaths->allShortestPaths[neighbouringVertexIndex].distance = currentDistanceToNeighbouringVertex;
//                vector<uint32_t> updatedPath = allPaths->allShortestPaths[currentVertexIndex].path;
//                updatedPath.push_back(neighbouringVertexIndex);
//                allPaths->allShortestPaths[neighbouringVertexIndex].path = updatedPath;
//                distancesPriorityQueue.push(make_pair(currentDistanceToNeighbouringVertex, neighbouringVertexIndex));
//            }
//        }
//    }
//    
//    
//    return allPaths;
//}


struct argumentStructureForShortestPathPreComputation{
    int threadId;
    //    unsigned granularity;
    GraphWithShortestPath *myGraph;
};

bool nextVertexToPreCompute(uint32_t &nextVertex){
//    uint32_t nextVertex = -1;
    bool hasNextVertex = false;
    pthread_mutex_lock(&mutexForNextVertexToPreCompute);
    if(nextVertexToPrecomputeIndex < verticesToPreCompute.size()){
        hasNextVertex = true;
        nextVertex = verticesToPreCompute[nextVertexToPrecomputeIndex];
        nextVertexToPrecomputeIndex++;
    }
    pthread_mutex_unlock(&mutexForNextVertexToPreCompute);
    return hasNextVertex;
}

void* preComputeFunction(void *arguments){
    struct argumentStructureForShortestPathPreComputation *argumentStructure = (struct argumentStructureForShortestPathPreComputation*) arguments;
//    int threadId = argumentStructure->threadId;
    GraphWithShortestPath *myGraph = argumentStructure->myGraph;
    SSSP sssp;
    uint32_t currentVertex;
    while (1) {
        bool hasNextVertex = nextVertexToPreCompute(currentVertex);
        if (!hasNextVertex) {
            break;
        }
        myGraph->precomputedOutgoingShortestPathsMap[currentVertex] = sssp.allShortestPathsFromVertex(currentVertex, *myGraph);
        myGraph->precomputedIncomingShortestPathsMap[currentVertex] = sssp.allShortestPathsToVertex(currentVertex, *myGraph);
    }

    return NULL;
}

void GraphWithShortestPath::precomputeShortestPathsParallel(vector<uint32_t> vertexIndices){
    
    nextVertexToPrecomputeIndex = 0;
    verticesToPreCompute = vertexIndices;

    pthread_t threads[Utils::getNumberOfThreads()];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (unsigned i=0; i<Utils::getNumberOfThreads(); i++) {
        struct argumentStructureForShortestPathPreComputation *arg = new struct argumentStructureForShortestPathPreComputation;
        arg->threadId = i;
        arg->myGraph = this;
        pthread_create(&threads[i], &attr, preComputeFunction, arg);
    }

    pthread_attr_destroy(&attr);
    for(unsigned j=0; j< Utils::getNumberOfThreads(); j++) {
        pthread_join(threads[j], NULL);
    }
    
    verticesToPreCompute.clear();

}

void GraphWithShortestPath::precomputeShortestPathsSerial(vector<uint32_t> vertexIndices){
//    for(uint32_t currentVertex : vertexIndices){
//        precomputedOutgoingShortestPathsMap[currentVertex] = allShortestPathsFromVertex(currentVertex);
//        precomputedIncomingShortestPathsMap[currentVertex] = allShortestPathsToVertex(currentVertex);
//    }
}



void GraphWithShortestPath::precomputeShortestPaths(int count, const map<uint32_t, vector<uint32_t> >& inDegreeToVerticesMap, const map<uint32_t, vector<uint32_t> >& outDegreeToVerticesMap){
    uint64_t timer = Utils::getDebugTimer();
    vector<uint32_t> verticesToCompute = getHighDegreeVertices(count, inDegreeToVerticesMap, outDegreeToVerticesMap);
    timer = Utils::getDebugTimer() - timer;
    cout << "Obtained high degree vertices: " << timer << "\n" <<endl;
    
//    precomputeShortestPathsSerial(verticesToCompute);
    precomputeShortestPathsParallel(verticesToCompute);
}

void GraphWithShortestPath::writeShortestPathToOutput(uint32_t source, uint32_t destination, ShortestPath shortestPath){
    vector<uint32_t> path;
    CSVRow logRow;
    stringstream sstream;
    if(shortestPath.distance != numeric_limits<uint32_t>::max())
        constructFullPath(shortestPath.joiningVertex, path);
    logRow.append(to_string(source) + "->" + to_string(destination));
    logRow.append(to_string(shortestPath.distance));
//    cout << path.size() << "\n" <<endl;
    for(uint32_t vertex : path)
        sstream<<vertex<<";";
    logRow.append(sstream.str());
    Utils::logCSVRow(OUTPUT_LOG, logRow);
    logRow.clear();
    sstream.clear();
    sstream.str("");
}

void GraphWithShortestPath::getAllVertexDegrees(map<uint32_t, vector<uint32_t> > &inDegreeToVerticesMap, map<uint32_t, vector<uint32_t> > &outDegreeToVerticesMap){
    vector<uint32_t> toReturn;
    for (uint32_t i=0; i<allVerticesVector.size(); i++) {
        uint32_t currentInDegree = (uint32_t)allVerticesVector[i].numberOfInEdges();
        uint32_t currentOutDegree = (uint32_t)allVerticesVector[i].numberOfOutEdges();

        map<uint32_t, vector<uint32_t> >::iterator it = inDegreeToVerticesMap.find(currentInDegree);
        if (it != inDegreeToVerticesMap.end()) {
            vector<uint32_t> &verticesVector = it->second;
            verticesVector.push_back(i);
        }
        else{
            vector<uint32_t> newVerticesVector;
            newVerticesVector.push_back(i);
            inDegreeToVerticesMap[currentInDegree] = newVerticesVector;
        }
        
        it = outDegreeToVerticesMap.find(currentOutDegree);
        if (it != outDegreeToVerticesMap.end()) {
            vector<uint32_t> &verticesVector = it->second;
            verticesVector.push_back(i);
        }
        else{
            vector<uint32_t> newVerticesVector;
            newVerticesVector.push_back(i);
            outDegreeToVerticesMap[currentOutDegree] = newVerticesVector;
        }
   }
}

vector<uint32_t> GraphWithShortestPath::getHighDegreeVertices(int count, const map<uint32_t, vector<uint32_t> >& inDegreeToVerticesMap, const map<uint32_t, vector<uint32_t> >& outDegreeToVerticesMap){
    vector<uint32_t> toReturn;
    for (auto iter = outDegreeToVerticesMap.rbegin(); iter != outDegreeToVerticesMap.rend(); ++iter) {
        vector<uint32_t> verticesVector = iter->second;
        for (auto vertex : verticesVector) {
            toReturn.push_back(vertex);
            if (--count == 0) {
                break;
            }
        }
        if (count == 0) {
            break;
        }
    }
    return toReturn;
}
