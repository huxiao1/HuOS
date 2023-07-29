#ifndef _GRAPH_H
#define _GRAPH_H


void init_graph(machbstart_t* mbsp);
void graph_t_init(graph_t* initp);
u32_t vfartolineadr(u32_t vfar);
void init_kinitfvram(machbstart_t* mbsp);

u32_t utf8_to_unicode(utf8_t* utfp,int* retuib);


#endif
