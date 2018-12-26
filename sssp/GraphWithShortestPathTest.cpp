//
//  GraphWithShortestPathTest.cpp
//  PageRankWithBarriers
//
//  Created by Mugilan M on 24/02/18.
//  Copyright © 2018 Mugilan M. All rights reserved.
//

#include "GraphWithShortestPathTest.hpp"

#include <stdio.h>
#include <fstream>
#define NUMBER_OF_GENERATED_INPUTS 100

typedef pair<uint32_t, uint32_t> iPair;

uint32_t edgeWeightUpperBound = 3;

uint32_t getWeight(uint32_t source, uint32_t destination){
    uint32_t weight;
    uint32_t constant1 = rand() % 100;
    uint32_t constant2 = 1;
    
    weight = (source + destination + constant1) % edgeWeightUpperBound + constant2;
    return weight;    
}

void initializeGraph(const char* inputFileName, GraphWithShortestPath &myGraph){
    ifstream inputFile;
    inputFile.open(inputFileName);
    
    if (!inputFileName) {
        Utils::logString(OUTPUT_LOG, "Error in command-line arguments. Invalid file name");
        Utils::logString(DEBUG_LOG, "Error in command-line arguments. Invalid file name");
    }
    uint32_t source, destination, weight;
    uint64_t timer = Utils::getTimer();
    string line;

    vector<string> tokens;
    int count1 = 0, count2 = 0;
    while (std::getline(inputFile, line))
    {
        count1++;
        // Process str
        if ((line[0] == '%') ||
            (line[0] == '#')){
            continue;
        }
        tokens.clear();
        string buf;
        stringstream ss(line);
        while (ss >> buf)
            tokens.push_back(buf);
        if (tokens.size() == 2) {
            count2++;
            source = stoi(tokens[0]);
            destination = stoi(tokens[1]);
            if (source == destination) {
                continue;
            }
            weight = getWeight(source, destination);
            myGraph.addEdge(source, destination, (uint32_t)weight);
        }
        else if (tokens.size() == 3){
            count2++;
            source = stoi(tokens[0]);
            destination = stoi(tokens[1]);
            if (source == destination) {
                continue;
            }
            weight = stoi(tokens[2]);
            myGraph.addEdge(source, destination, (uint32_t)weight);
        }
        else{
            cout << "Incorrect input format \n" <<endl;
//            inputFile.close();
//            exit(1);
        }
    }

    inputFile.close();

    cout << "count1 : " << count1 <<endl;
    cout << "count2 : " << count2 <<endl;
    timer = Utils::getTimer() - timer;
    cout<< "initialized graph : " << timer << "\n" << endl;

}

vector<iPair> generateInputs(GraphWithShortestPath &myGraph){
    vector<iPair> inputs;
    uint32_t source, destination;
    for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
        source = rand() % myGraph.allVerticesVector.size();
        destination = rand() % myGraph.allVerticesVector.size();
        inputs.push_back(make_pair(source, destination));
    }
    return inputs;
}


vector<iPair> generateInputs2(int choice, GraphWithShortestPath &myGraph, const char* queriesFilePath, map<uint32_t, vector<uint32_t> > inDegreeToVerticesMap, map<uint32_t, vector<uint32_t> > outDegreeToVerticesMap, size_t numberOfVertices){
    vector<iPair> inputs;
    
    
    vector<uint32_t> highInDegreeVertices;
    vector<uint32_t> highOutDegreeVertices;
    vector<uint32_t> lowInDegreeVertices;
    vector<uint32_t> lowOutDegreeVertices;
    vector<uint32_t> medianInDegreeVertices;
    vector<uint32_t> medianOutDegreeVertices;
    
    uint32_t verticesCountMidValue = (uint32_t)numberOfVertices/2;
    uint32_t initialIndexForMid = verticesCountMidValue - ((uint32_t)numberOfVertices / 40);
    uint32_t currentVerticesCount = 0;
    
//    int size = NUMBER_OF_GENERATED_INPUTS;
    uint32_t lowRange = (uint32_t)numberOfVertices / 20;
    uint32_t highRange = (uint32_t)numberOfVertices / 25;
    uint32_t midRange = (uint32_t)numberOfVertices / 15;
    
    for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
        uint32_t value = rand() % lowRange;
        uint32_t temp;
        
        
        //        lowInDegreeVertices
        temp = value;
        for (map<uint32_t, vector<uint32_t> >::iterator it = inDegreeToVerticesMap.begin(); it != inDegreeToVerticesMap.end() ; it++) {
            if(it->first == 0)
                continue;
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            if (temp < currentSize) {
                lowInDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }

        //        lowOutDegreeVertices
        temp = value;
        for (map<uint32_t, vector<uint32_t> >::iterator it = outDegreeToVerticesMap.begin(); it != outDegreeToVerticesMap.end() ; it++) {
            if(it->first == 0)
                continue;
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            if (temp < currentSize) {
                lowOutDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }

        value = rand() % highRange;
        //        highInDegreeVertices
        temp = value;
        for (map<uint32_t, vector<uint32_t> >::reverse_iterator it = inDegreeToVerticesMap.rbegin(); it != inDegreeToVerticesMap.rend() ; it++) {
//            uint32_t degree = it->first;
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            if (temp < currentSize) {
                highInDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }

        //        highOutDegreeVertices
        temp = value;
        for (map<uint32_t, vector<uint32_t> >::reverse_iterator it = outDegreeToVerticesMap.rbegin(); it != outDegreeToVerticesMap.rend() ; it++) {
//            uint32_t degree = it->first;
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            if (temp < currentSize) {
                highOutDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }
        
        //        medianInDegreeVertices
        
        value = rand() % midRange;
        temp = value;
        currentVerticesCount = 0;
        
        for (map<uint32_t, vector<uint32_t> >::iterator it = inDegreeToVerticesMap.begin(); it != inDegreeToVerticesMap.end() ; it++) {
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            currentVerticesCount += currentSize;
            if (currentVerticesCount < initialIndexForMid) {
                continue;
            }
            if (temp < currentSize) {
                medianInDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }

        //        medianInDegreeVertices
        
        value = rand() % midRange;
        temp = value;
        currentVerticesCount = 0;
        
        for (map<uint32_t, vector<uint32_t> >::iterator it = outDegreeToVerticesMap.begin(); it != outDegreeToVerticesMap.end() ; it++) {
            vector<uint32_t> vertexVector = it->second;
            uint32_t currentSize = (uint32_t)vertexVector.size();
            currentVerticesCount += currentSize;
            if (currentVerticesCount < initialIndexForMid) {
                continue;
            }
            if (temp < currentSize) {
                medianOutDegreeVertices.push_back(vertexVector[temp]);
                break;
            }
            temp = temp - currentSize;
        }


    }
    
    switch(choice){
        case 1: {
            // High Indegree to High OutDegree
            cout << "High Indegree to High OutDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = highInDegreeVertices[i];
                destination = highOutDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 2: {
            // High Outdegree to High InDegree
            cout << "High Outdegree to High InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = highOutDegreeVertices[i];
                destination = highInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 3: {
            // Low Indegree to Low OutDegree
            cout << "Low Indegree to Low OutDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = lowInDegreeVertices[i];
                destination = lowOutDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 4: {
            // Low Outdegree to Low InDegree
            cout << "Low Outdegree to Low InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = lowOutDegreeVertices[i];
                destination = lowInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 5: {
            // Low Outdegree to High InDegree
            cout << "Low Outdegree to High InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = lowOutDegreeVertices[i];
                destination = highInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 6: {
            // High Outdegree to Low InDegree
            cout << "High Outdegree to Low InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = highOutDegreeVertices[i];
                destination = lowInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 7: {
            // Median Outdegree to Median InDegree
            cout << "Median Outdegree to Median InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = medianOutDegreeVertices[i];
                destination = medianInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 8: {
            // Median Outdegree to High InDegree
            cout << "Median Outdegree to High InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = medianOutDegreeVertices[i];
                destination = highInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 9: {
            // Median Outdegree to Low InDegree
            cout << "Median Outdegree to Low InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = medianOutDegreeVertices[i];
                destination = lowInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 10: {
            // High Outdegree to Median InDegree
            cout << "High Outdegree to Median InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = highOutDegreeVertices[i];
                destination = medianInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 11: {
            // Low Outdegree to Median InDegree
            cout << "Low Outdegree to Median InDegree" <<endl;
            for (int i=0; i<NUMBER_OF_GENERATED_INPUTS; i++) {
                uint32_t source, destination;
                source = lowOutDegreeVertices[i];
                destination = medianInDegreeVertices[i];
                inputs.push_back(make_pair(source, destination));
            }
            break;
        }
        case 12: {
            // Use inputFile
            cout<< "Using inputFile : "<< queriesFilePath <<endl;
            inputs = generateInputs(myGraph);
            uint32_t source, destination;
            ifstream inputFile;
            inputFile.open(queriesFilePath);
            while (inputFile >> source) {
                if(inputFile >> destination){
                    inputs.insert(inputs.begin(), (make_pair(source, destination)));
                    //            inputs.push_back(make_pair(source, destination));
                }
                else{
                    break;
                }
            }
            inputFile.close();
            break;
        }
        default:{ // Invalid choice of inputs
            cout<< "Invalid choice of inputs.. Using randomly generated inputs" <<endl;
            inputs = generateInputs(myGraph);
            
        }
    }
    
    return inputs;

}


void shrinkGraph(GraphWithShortestPath &myGraph){
    for (unsigned i=0; i<myGraph.allVerticesVector.size(); i++) {
        GraphVertex &vertex = myGraph.allVerticesVector[i];
        vertex.pruneVectors();
    }
}

void writeTitleRow(){
    CSVRow logRow;
    logRow.append("Query");
    logRow.append("Src->Dist");
//    logRow.append("dist - Serial execution Unidirectional pq");
//    logRow.append("Serial execution Unidirectional pq- time");
//
//    logRow.append("dist - Serial execution bidirectional fifo");
//    logRow.append("Serial execution bidirectional fifo - time");
    
    logRow.append("dist - FIFO parallel execution - bidirectional");
    logRow.append("FIFO parallel execution - bidirectional - time");
//    logRow.append("bound - FIFO parallel execution - bidirectional with bounds");
//    logRow.append("dist - FIFO parallel execution - bidirectional with bounds");
//    logRow.append("FIFO parallel execution - bidirectional with bounds - time");

    logRow.append("dist - FIFO parallel execution - unidirectional");
    logRow.append("FIFO parallel execution - unidirectional - time");
//    logRow.append("bound - FIFO parallel execution - unidirectional with bounds");
//    logRow.append("dist - FIFO parallel execution - unidirectional with bounds");
//    logRow.append("FIFO parallel execution - unidirectional with bounds - time");

    logRow.append("dist - PQ parallel execution - bidirectional");
    logRow.append("PQ parallel execution - bidirectional - time");
//    logRow.append("dist - PQ parallel execution - bidirectional with bounds");
//    logRow.append("PQ parallel execution - bidirectional with bounds- time");

    logRow.append("dist - PQ parallel execution - unidirectional");
    logRow.append("PQ parallel execution - unidirectional - time");
//    logRow.append("dist - PQ parallel execution - unidirectional with bounds");
//    logRow.append("PQ parallel execution - unidirectional with bounds- time");

    Utils::logCSVRow(DEBUG_LOG, logRow);
}


void myTest8(const char* inputFileName, const char* queriesFilePath, int numberOfHighDegreeVertices, int totalNumberOfQueries, int choiceOfInput, int  edgeWeighUpperLimit){
    // TODO : Move the graph creation part to modularized location
    
    edgeWeightUpperBound = edgeWeighUpperLimit;
    
    GraphWithShortestPath myGraph(Utils::numberOfThreads);

    initializeGraph(inputFileName, myGraph);
    
//    cout << "Edges : " << myGraph.allEdges.size() <<endl;
//    cout << "Vertices : " << myGraph.allVerticesVector.size() <<endl;
    shrinkGraph(myGraph);
    
    vector<iPair> inputs;
    
    // randomly generate 100 input queries
//    inputs = generateInputs(myGraph);
    
    
    
    cout<<"Shortest paths\n";
    CSVRow logRow;
    stringstream sstream;
    
    int numberOfQueries = totalNumberOfQueries;
    int numberOfPathLessQueries = 0;
    
    uint64_t timer = Utils::getTimer();
    map<uint32_t, vector<uint32_t> > inDegreeToVerticesMap, outDegreeToVerticesMap;
    myGraph.getAllVertexDegrees(inDegreeToVerticesMap, outDegreeToVerticesMap);
    myGraph.precomputeShortestPaths(numberOfHighDegreeVertices, inDegreeToVerticesMap, outDegreeToVerticesMap);
    timer = Utils::getTimer() - timer;
    cout<<"Completed computing Shortest paths : " << timer << "\n" << endl;

    
    inputs = generateInputs2(choiceOfInput, myGraph, queriesFilePath, inDegreeToVerticesMap, outDegreeToVerticesMap, myGraph.allVerticesVector.size());
    
    myGraph.initializeShortestPathValues();
    writeTitleRow();
    
    
    timer = Utils::getTimer();
    for (unsigned i=0; i<inputs.size(); i++) {
        if (numberOfQueries == 0) {
            break;
        }
        uint32_t source = inputs[i].first;
        uint32_t dest = inputs[i].second;
        
        logRow.append(to_string(totalNumberOfQueries - numberOfQueries + 1));
        stringstream sstream;
        sstream<<source << "->" << dest;
        logRow.append(sstream.str());
        
        
        ShortestPath shortestPath;
        uint64_t executionTimer;
//        uint32_t upperBound;
        
        
//        // Serial execution Unidirectional
//        resetFinalValues();
//        executionTimer = Utils::getTimer();
//        shortestPath = myGraph.getShortestPath_serial(source, dest);
//        executionTimer =  Utils::getTimer() - executionTimer;
//        logRow.append(to_string(shortestPath.distance));
//        logRow.append(to_string(executionTimer));
//        myGraph.writeShortestPathToOutput(source, dest, shortestPath);
//
//        // Serial execution bidirectional fifo
//        resetFinalValues();
//        executionTimer = Utils::getTimer();
//        shortestPath = myGraph.getShortestPath_serial_bidirectional_fifo(source, dest);
//        executionTimer =  Utils::getTimer() - executionTimer;
//        logRow.append(to_string(shortestPath.distance));
//        logRow.append(to_string(executionTimer));
//        myGraph.writeShortestPathToOutput(source, dest, shortestPath);


        // FIFO parallel execution - bidirectional
        resetFinalValues();
        executionTimer = Utils::getTimer();
        shortestPath = myGraph.getShortestPath3(source, dest, false);
        executionTimer =  Utils::getTimer() - executionTimer;
        logRow.append(to_string(shortestPath.distance));
        logRow.append(to_string(executionTimer));
        myGraph.writeShortestPathToOutput(source, dest, shortestPath);

//        // FIFO parallel execution - bidirectional with bounds
//        resetFinalValues();
//        executionTimer = Utils::getTimer();
//
//        shortestPath = myGraph.getShortestPathWithBounds(source, dest, false, false, upperBound);
//        executionTimer =  Utils::getTimer() - executionTimer;
//        logRow.append(to_string(upperBound));
//        if (upperBound < shortestPath.distance) {
//            cout << "Error" << endl;
//            cout << "UpperBound : " << upperBound << endl;
//            cout << "Distance : " << shortestPath.distance << endl;
//        }
//        logRow.append(to_string(shortestPath.distance));
//        logRow.append(to_string(executionTimer));
//        myGraph.writeShortestPathToOutput(source, dest, shortestPath);

        
//        FIFO parallel execution - unidirectional
        resetFinalValues();
        executionTimer = Utils::getTimer();
        shortestPath = myGraph.getShortestPath_parallel_unidirectional(source, dest, false);
        executionTimer =  Utils::getTimer() - executionTimer;
        logRow.append(to_string(shortestPath.distance));
        logRow.append(to_string(executionTimer));
        myGraph.writeShortestPathToOutput(source, dest, shortestPath);

////         FIFO parallel execution - unidirectional with bounds
//        resetFinalValues();
//        executionTimer = Utils::getTimer();
//        shortestPath = myGraph.getShortestPathWithBounds(source, dest, false, true, upperBound);
//        executionTimer =  Utils::getTimer() - executionTimer;
//        logRow.append(to_string(upperBound));
//        if (upperBound < shortestPath.distance) {
//            cout << "Error" << endl;
//            cout << "UpperBound : " << upperBound << endl;
//            cout << "Distance : " << shortestPath.distance << endl;
//        }
//        logRow.append(to_string(shortestPath.distance));
//        logRow.append(to_string(executionTimer));
//        myGraph.writeShortestPathToOutput(source, dest, shortestPath);

        
        // PQ parallel execution - bidirectional
        resetFinalValues();
        executionTimer = Utils::getTimer();
        shortestPath = myGraph.getShortestPath3(source, dest, true);
        executionTimer =  Utils::getTimer() - executionTimer;
        logRow.append(to_string(shortestPath.distance));
        logRow.append(to_string(executionTimer));
        myGraph.writeShortestPathToOutput(source, dest, shortestPath);
        
        
//        // PQ parallel execution - bidirectional with bound
//        resetFinalValues();
//        executionTimer = Utils::getTimer();
//        shortestPath = myGraph.getShortestPathWithBounds(source, dest, true, upperBound);
//        executionTimer =  Utils::getTimer() - executionTimer;
//        logRow.append(to_string(upperBound));
//        logRow.append(to_string(shortestPath.distance));
//        logRow.append(to_string(executionTimer));
//        myGraph.writeShortestPathToOutput(source, dest, shortestPath);

//       PQ parallel execution - unidirectional
        resetFinalValues();
        executionTimer = Utils::getTimer();
        shortestPath = myGraph.getShortestPath_parallel_unidirectional(source, dest, true);
        executionTimer =  Utils::getTimer() - executionTimer;
        logRow.append(to_string(shortestPath.distance));
        logRow.append(to_string(executionTimer));
        myGraph.writeShortestPathToOutput(source, dest, shortestPath);
        

        
//        ShortestPath shortestPath = myGraph.getShortestPath_all(source, dest);
//        ShortestPath shortestPath = myGraph.getShortestPath_serial_all(source, dest);

        if (shortestPath.distance != numeric_limits<uint32_t>::max()) {
            numberOfQueries--;
        }
        else
        {
            numberOfPathLessQueries++;
            if (numberOfPathLessQueries == 30) {
                break;
            }
//            cout << "No shortest path found.. ignoring this pair" <<endl;
        }
        Utils::logCSVRow(DEBUG_LOG, logRow);
        logRow.clear();
    }
    timer = Utils::getTimer() - timer;
    cout<<"Completed computing queries : " << timer << "\n" << endl;

    myGraph.destroyShortestPathValues();
    
    //    vector<uint32_t> outputVector = myGraph.(10);
    
}


void shortestPath_Serial(const char* inputFileName, const char* queriesFilePath){
    // TODO : Move the graph creation part to modularized location
    ifstream inputFile;
    inputFile.open(inputFileName);
    
    if (!inputFileName) {
        Utils::logString(OUTPUT_LOG, "Error in command-line arguments. Invalid file name");
        Utils::logString(DEBUG_LOG, "Error in command-line arguments. Invalid file name");
    }
    
    GraphWithShortestPath myGraph(Utils::numberOfThreads);
    int source, destination;
    
    // Assumption the vertices are from 0 to n-1.
    
    while (inputFile >> source) {
        if(inputFile >> destination){
            // We are using an undirected graph
            myGraph.addEdge(source, destination);
//            myGraph.addEdge(destination, source);
            
        }else{
            break;
        }
    }
    cout<< "initialized graph" << endl;
    inputFile.close();
    typedef pair<uint32_t, uint32_t> iPair;
    
    vector<iPair> inputs;
    inputFile.open(queriesFilePath);
    while (inputFile >> source) {
        if(inputFile >> destination){
            inputs.push_back(make_pair(source, destination));
        }
        else{
            break;
        }
    }
    inputFile.close();
    
    cout<<"Shortest paths\n";
    CSVRow logRow;
    stringstream sstream;
    
    for (unsigned i=0; i<inputs.size(); i++) {
        uint32_t source = inputs[i].first;
        uint32_t dest = inputs[i].second;
        logRow.append("STARTING " + to_string(i));
        Utils::logCSVRow(DEBUG_LOG, logRow);
        logRow.clear();
        
        ShortestPath shortestPath = myGraph.getShortestPath_serial(source, dest);
//        ShortestPath shortestPath = myGraph.getShortestPath(source, dest);
        Utils::refreshLocks();
        
        logRow.append(to_string(source) + "->" + to_string(dest));
        logRow.append(to_string(shortestPath.distance));
//        for(uint32_t vertex : shortestPath.path)
//            sstream<<vertex<<";";
//        logRow.append(sstream.str());
        Utils::logCSVRow(OUTPUT_LOG, logRow);
        logRow.clear();
        sstream.clear();
        sstream.str("");
        
    }
    
    //    vector<uint32_t> outputVector = myGraph.(10);
    
}


