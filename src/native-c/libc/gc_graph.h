#ifndef AM_LANG_GC_GRAPH_H
#define AM_LANG_GC_GRAPH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#define GC_GRAPH_MMAP 1
#endif
// Pure reachability analysis. This module neither modifies nor frees application
// objects. Each edge supplied by the caller must represent one owning reference.
typedef struct {
    void *object;
    long references, internal;
    int live;
} cycle_node;
typedef struct {
    cycle_node *nodes;
    size_t count, *table, table_size, *queue, queued;
    int marking;
    void *workspace;
    size_t workspace_bytes;
} cycle_graph;
static size_t cycle_hash(void *p) {
    uintptr_t v=(uintptr_t)p >> 3;
    v ^= v >> 30; v *= UINT64_C(0xbf58476d1ce4e5b9); v ^= v >> 27;
    return (size_t)v;
}
static int cycle_init(cycle_graph *g, size_t count) {
    memset(g,0,sizeof(*g)); g->count=count; g->table_size=16;
    if(count > SIZE_MAX / (8 * sizeof(cycle_node))) return 0;
    while(g->table_size < count*2) g->table_size*=2;
    size_t nodes_bytes=(count+1)*sizeof(cycle_node);
    size_t table_bytes=g->table_size*sizeof(size_t);
    g->workspace_bytes=nodes_bytes+table_bytes+(count+1)*sizeof(size_t);
    // Collection sizes change frequently. Some malloc implementations cache
    // freed large allocations, retaining hundreds of MB of dirty scratch pages.
    // A private mapping makes disposal release these pages, including compressed
    // pages, without touching allocator state belonging to another thread.
#ifdef GC_GRAPH_MMAP
    g->workspace=mmap(NULL,g->workspace_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(g->workspace==MAP_FAILED) { g->workspace=NULL; return 0; }
#else
    g->workspace=calloc(1,g->workspace_bytes);
    if(!g->workspace) return 0;
#endif
    g->nodes=g->workspace;
    g->table=(size_t*)((char*)g->workspace+nodes_bytes);
    g->queue=(size_t*)((char*)g->workspace+nodes_bytes+table_bytes);
    return 1;
}
static void cycle_dispose(cycle_graph *g) {
    if(!g->workspace) return;
#ifdef GC_GRAPH_MMAP
    munmap(g->workspace,g->workspace_bytes);
#else
    free(g->workspace);
#endif
    g->workspace=NULL;
}
static void cycle_add(cycle_graph *g,size_t i,void *p,long refs) {
    g->nodes[i].object=p; g->nodes[i].references=refs;
    size_t slot=cycle_hash(p)&(g->table_size-1);
    while(g->table[slot]) slot=(slot+1)&(g->table_size-1);
    g->table[slot]=i+1;
}
static int cycle_contains(cycle_graph *g,void *p) {
    size_t slot=cycle_hash(p)&(g->table_size-1);
    while(g->table[slot]) {
        if(g->nodes[g->table[slot]-1].object==p) return 1;
        slot=(slot+1)&(g->table_size-1);
    }
    return 0;
}
static void cycle_edge(cycle_graph *g,void *p) {
    if(!p) return;
    size_t slot=cycle_hash(p)&(g->table_size-1);
    while(g->table[slot]) {
        size_t i=g->table[slot]-1;
        if(g->nodes[i].object==p) {
            if(!g->marking) g->nodes[i].internal++;
            else if(!g->nodes[i].live) { g->nodes[i].live=1; g->queue[g->queued++]=i; }
            return;
        }
        slot=(slot+1)&(g->table_size-1);
    }
}
static void cycle_roots(cycle_graph *g) {
    g->marking=1;
    for(size_t i=0;i<g->count;i++) {
        // Any external owner, or inconsistent count, is conservatively live.
        if(g->nodes[i].references != g->nodes[i].internal) {
            g->nodes[i].live=1; g->queue[g->queued++]=i;
        }
    }
}
#endif
