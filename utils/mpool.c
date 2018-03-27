#include <string.h>
#include <stdlib.h>
#include <obstack.h>
#include "defs.h"
#include "mpool.h"
#include "map.h"
#undef mp_alloc
#undef mp_free
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free xfree

#ifdef OBSTACK
static struct obstack obs;
#endif
static struct Map_ map;

struct Recycle {
  struct Recycle *next;
};

struct pool {
  uint32_t  obj_sz;
  uint32_t  blk_sz;
  uint32_t  obj_id;
  int32_t   blk_id;
  uint32_t  nblk;
  struct Recycle  *next;
  uint8_t **data;
};

__attribute__((constructor(200)))
void mpool_ini() {
  map_init(&map);
  obstack_init(&obs);
}

__attribute__((destructor(200)))
void mpool_end() {
  LOOP_OPTIM
  for(m_uint i = map_size(&map) + 1; --i;)
    mp_end((struct pool*)VVAL(&map, i - 1));
  map_release(&map);
#ifdef OBSTACK
  obstack_free(&obs, NULL);
#endif
}

static struct pool* mp_get(const uint32_t obj_sz, const uint32_t blk_sz) {
  LOOP_OPTIM
  for(m_uint i = map_size(&map) + 1; --i;) {
    if(VKEY(&map, i - 1) == obj_sz)
      return (struct pool*)VVAL(&map, i - 1);
  }
#ifdef OBSTACK
  struct pool* p = (struct pool*)obstack_alloc(&obs, sizeof(struct pool));
#else
  struct pool* p = (struct pool*)xmalloc(sizeof(struct pool));
#endif
  p->obj_sz = obj_sz;
  p->blk_sz = blk_sz;
  p->obj_id = blk_sz - 1;
  p->blk_id = -1;
  p->nblk   = 1;
  p->next   = NULL;
  p->data   = (uint8_t**)xcalloc(1, sizeof(uint8_t*));
  map_set(&map, obj_sz, (vtype)p);
  return p;
}

ANN struct pool* mp_ini(const uint32_t obj_sz, const uint32_t blk_sz) {
  return mp_get((obj_sz + 3) & 0xfffffffc, blk_sz);
}

ANN void mp_end(struct pool *p) {
  for(uint32_t i = p->nblk + 1; --i;)
    free(p->data[i-1]);
  free(p->data);
#ifndef OBSTACK
  free(p->data);
#endif
}

ANN void *mp_alloc(struct pool *p) {
  if(p->next) {
    void *recycle = p->next;
    p->next = p->next->next;
    memset(recycle, 0, p->obj_sz);
    return recycle;
  }
  if(++p->obj_id == p->blk_sz) {
    p->obj_id = 0;
    if(++p->blk_id == (int32_t)p->nblk) {
      p->nblk <<= 1;
      p->data = (uint8_t**)xrealloc(p->data, sizeof(uint8_t*)* p->nblk);
      memset(p->data + (p->nblk >> 1), 0, (p->nblk - (p->nblk >> 1)) * sizeof(uint8_t*));
    }
    p->data[p->blk_id] = calloc(p->obj_sz, p->blk_sz);
  }
  return p->data[p->blk_id] + p->obj_id * p->obj_sz;
}

ANN void mp_free(struct pool *p, void *ptr) {
  struct Recycle* next = p->next;
  p->next = ptr;
  p->next->next = next;
}
