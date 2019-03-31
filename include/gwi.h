#ifndef __GWI
#define __GWI
typedef struct {
  Type_Decl t;
  struct Var_Decl_List_ list;
  struct Var_Decl_ var;
  struct Exp_ exp;
  size_t array_depth;
} DL_Var;

typedef struct {
  m_str name;
  m_str type;
} DL_Value;

typedef struct {
  m_str    name;
  m_str    type;
  f_xfun   addr;
  DL_Value args[DLARG_MAX];
  uint narg;
} DL_Func;

typedef struct {
  Operator op;
  m_str ret, lhs, rhs;
  Type   (*ck)(Env, void*);
  m_bool (*em)(Emitter, void*);
  m_bool mut;
} DL_Oper;

typedef struct {
  m_str* list;
  size_t n;
} Templater;

typedef struct {
  m_str t;
  ID_List base, curr;
  struct Vector_ addr;
} DL_Enum;

typedef struct {
  Symbol xid;
  Decl_List list;
} DL_Union;

struct Gwi_{
  VM* vm;
  Emitter emit;
  Env env;
  Class_Body body;
  DL_Var var;
  DL_Func func;
  DL_Enum enum_data;
  DL_Union union_data;
  DL_Oper oper;
  void* addr;
  Templater templater;
};
#endif
