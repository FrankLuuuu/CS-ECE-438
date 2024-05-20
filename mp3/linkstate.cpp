#include <cstdio>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <climits>

using namespace std;

struct node_t {
    vector<int> D;
    vector<int> p;
    int source;
};

typedef pair<int, int> pair_t;

void add_edge(vector<pair_t> adj[], int x, int y, int z) {
    adj[y].push_back(make_pair(x, z));
    adj[x].push_back(make_pair(y, z));
}

void delete_edge(vector<pair_t> adj[], int x, int y) {
    for(int i = 0; i < adj[x].size(); i++) {
        if(adj[x][i].first != y)
            continue;
        else 
            adj[x].erase(adj[x].begin() + i);
    }
    for(int i = 0; i < adj[y].size(); i++) {
        if(adj[y][i].first != x)
            continue;
        else 
            adj[y].erase(adj[y].begin() + i);
    }
}

void dijkstras(node_t* node, vector<pair_t> adj[], int count) {
    int curr, a, b;
    priority_queue<pair_t, vector<pair_t>, greater<pair_t>> closest;
    vector<bool> visited(count, false);

    if(!node)
        return;

    node->p.clear();
    node->p.resize(count + 1, -1);
    node->D.clear();
    node->D.resize(count + 1, INT_MAX);
    node->D[node->source] = 0;

    closest.push(make_pair(0, node->source));
    while(!closest.empty()) {
        curr = closest.top().second;
        closest.pop();

        if(!visited[curr]) {
            for(int i = 0; i < adj[curr].size(); i++) {
                a = adj[curr][i].first;
                b = adj[curr][i].second;

                if(node->D[curr] + b < node->D[a]) {
                    node->D[a] = node->D[curr] + b;
                    closest.push(make_pair(node->D[a], a));
                    node->p[a] = curr;
                }
                else if(node->D[curr] + b == node->D[a] && curr < node->p[a]) {
                    closest.push(make_pair(node->D[a], a));
                    node->p[a] = curr;
                }
            }
        }

        visited[curr] = true;
    }
}

vector<int> get_path(int src, int dest, node_t* nodes) {
    vector<int> path;

    int next = dest;
    path.push_back(dest);
    
    while(nodes[src].p[next] != src) {
        if(!nodes[src].p[next]) {
            next = src;
            path.push_back(next);
            break;
        }
        next = nodes[src].p[next];
        path.push_back(next);
    }

    return path;
}

void forwarding_table(FILE* fpOut, int count, node_t* nodes) {
    for(int i = 1; i <= count; i++) 
        for(int j = 1; j <= count; j++) 
            if(nodes[i].D[j] != INT_MAX) 
                fprintf(fpOut, "%d %d %d\n", j, get_path(i, j, nodes).back(), nodes[i].D[j]);
}

void message(FILE* fpOut, node_t* nodes, string line) {
    string message;
    int src, dest, cost;
    vector<int> path;

    src = stoi(line.substr(0, line.find(" ")));
    dest = stoi(line.substr(line.find(" ") + 1, line.find(" ", line.find(" ") + 1)));
    message = line.substr(line.find(" ", line.find(" ") + 1) + 1);
    cost = nodes[src].D[dest];

    if(cost != INT_MAX) {
        path = get_path(src, dest, nodes);
        path.push_back(src); 

        fprintf(fpOut, "from %d to %d cost %d hops ", src, dest, cost);

        for(int i = path.size() - 1; i >= 0; i--) 
            if(path[i] != dest) 
                fprintf(fpOut, "%d ", path[i]);

        fprintf(fpOut, "message %s\n", message.c_str());

        return;
    }
    
    fprintf(fpOut, "from %d to %d cost infinite hops unreachable message %s\n", src, dest, message.c_str());
}

bool update_cost(int x, int y, int new_cost, vector<pair_t> adj[]) {
    int curr;
    bool found = false;
    for(int i = 0; i < adj[x].size(); i++) {
        curr = adj[x][i].first;
        if(curr == y) {
            adj[x][i].second = new_cost;
            found = true;
            break;
        }
    }
    for(int i = 0; i < adj[y].size(); i++) {
        curr = adj[y][i].first;
        if(curr == x) {
            adj[y][i].second = new_cost;
            found = true;
            break;
        }
    }

    return found;
}


int main(int argc, char** argv) {
    string node1, node2, weight, line;
    int count = 0;
    FILE *fpOut;

    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    fpOut = fopen("output.txt", "w");

    ifstream input(argv[1]);
    while(input >> node1 >> node2 >> weight) {
        int node1_int = stoi(node1);
        int node2_int = stoi(node2);

        if(node1_int > count) 
            count = node1_int;
        if(node2_int > count) 
            count = node2_int;
    }
    input.close();

    vector<pair_t> adj[count + 1];
    input.open(argv[1]);
    while(input >> node1 >> node2 >> weight) {
        if(stoi(weight) != -999) 
            add_edge(adj, stoi(node1), stoi(node2), stoi(weight));
    }
    input.close();
    
    node_t* nodes = new node_t[count + 1];
    for(int i = 1; i <= count; i++) {
        nodes[i].D.resize(count + 1, INT_MAX);
        nodes[i].p.resize(count + 1, -1);
        nodes[i].source = i;
    }

    for(int i = 1; i <= count; i++)
        dijkstras(&nodes[i], adj, count);

    forwarding_table(fpOut, count, nodes);

    input.open(argv[2]);
    while(getline(input, line))
        message(fpOut, nodes, line);
    input.close();

    ifstream changes(argv[3]);
    while(changes >> node1 >> node2 >> weight) {
        int src = stoi(node1);
        int dest = stoi(node2);
        int cost = stoi(weight);

        if(cost == -999)
            delete_edge(adj, src, dest);
        else
            if(!update_cost(src, dest, cost, adj))
                add_edge(adj, src, dest, cost);

        for(int i = 1; i <= count; i++)
            dijkstras(&nodes[i], adj, count);

        forwarding_table(fpOut, count, nodes);

        input.open(argv[2]);
        while(getline(input, line))
            message(fpOut, nodes, line);
        input.close();
    }
    changes.close();

    fclose(fpOut);

    // You may choose to use std::fstream instead
    // std::ofstream fpOut("output.txt");

    return 0;
}
