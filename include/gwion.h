#ifndef __GWION
#define __GWION

typedef struct Gwion_* Gwion;
#include "plug.h"
#include "driver.h"
struct Arg_;
struct Gwion_ {
  PlugInfo* plug;
  Env      env;
  Emitter  emit;
  VM*      vm;
  struct Map_ freearg;
  SymTable *st;
  MemPool mp;
  struct Vector_ child;
};

ANN m_bool gwion_ini(const Gwion, struct Arg_*);
ANN VM* gwion_cpy(const VM*);
ANN void gwion_run(const Gwion gwion);
ANN void gwion_end(const Gwion gwion);
void free_code_instr(const Vector v, const Gwion gwion);
#endif
