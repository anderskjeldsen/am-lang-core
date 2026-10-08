#include "../../src/native-c/libc/gc_graph.h"
#include <assert.h>
#include <stdio.h>
#define N 64
int main(void) {
    unsigned seed=721;
    // Compare subtraction-based roots with an independent graph reachability
    // oracle over random cycles, shared edges, external/unknown native owners.
    for(int trial=0;trial<10000;trial++) {
        int objects[N], edges[N][N]={0}, external[N], expected[N]={0};
        long refs[N]={0}; cycle_graph g;
        assert(cycle_init(&g,N));
        for(int i=0;i<N;i++) {
            seed=seed*1664525+1013904223; external[i]=(seed>>24)%17==0;
            refs[i]=external[i]; expected[i]=external[i];
        }
        for(int i=0;i<N;i++) for(int j=0;j<N;j++) {
            seed=seed*1664525+1013904223;
            edges[i][j]=((seed>>24)%53==0)?1+(seed%3):0;
            refs[j]+=edges[i][j];
        }
        for(int i=0;i<N;i++) cycle_add(&g,i,&objects[i],refs[i]);
        for(int i=0;i<N;i++) for(int j=0;j<N;j++) for(int k=0;k<edges[i][j];k++) cycle_edge(&g,&objects[j]);
        cycle_roots(&g);
        for(size_t q=0;q<g.queued;q++) {
            size_t i=g.queue[q];
            for(int j=0;j<N;j++) if(edges[i][j]) cycle_edge(&g,&objects[j]);
        }
        for(int pass=0;pass<N;pass++) for(int i=0;i<N;i++) if(expected[i])
            for(int j=0;j<N;j++) if(edges[i][j]) expected[j]=1;
        for(int i=0;i<N;i++) assert(g.nodes[i].live==expected[i]);
        cycle_dispose(&g);
    }
    puts("10,000 cyclic graphs match independent reachability oracle");
}
