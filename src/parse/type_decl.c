#include <stdlib.h>
#include <string.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "oo.h"
#include "env.h"
#include "type.h"

ANN Type type_decl_resolve(const Env env, const Type_Decl* td) {
  Type t = find_type(env, td->xid);
  CHECK_OO(t)
  CHECK_OO((t = scan_type(env, t, td)))
  return !td->array ? t : array_type(t, td->array->depth);
}

struct td_info {
  Type_List tl;
  m_str str;
  size_t len;
  size_t cap;
};

ANN static inline m_bool td_add(struct td_info* info, const char c) {
  return *(info->str + info->len++ - 1) = c;
}

ANN static inline void td_check(struct td_info* info, const size_t len) {
  if(info->len + len >= info->cap) {
    while(info->len + len >= info->cap)
      info->cap <<= 1;
    info->str = (m_str)xrealloc(info->str, info->cap);
  }
}

ANEW ANN static m_str td2str(const Env env, const Type_Decl* td);
ANN static void td_info_run(const Env env, struct td_info* info) {
  Type_List tl = info->tl;
  do {
    m_str name = td2str(env, tl->td);
    const size_t nlen = strlen(name);
    td_check(info, nlen + !!tl->next);
    strcpy(info->str + info->len -1, name);
    info->len += nlen;
    xfree(name);
  } while((tl = tl->next) && td_add(info, ','));
}

ANEW ANN static m_str td2str(const Env env, const Type_Decl* td) {
  m_uint depth = td->array ? td->array->depth : 0;
  size_t l = id_list_len(td->xid)  + depth *2;
  struct td_info info = { td->types, (m_str)xmalloc(l), l, l };
  type_path(info.str, td->xid);
  while(depth--) { td_add(&info, '['); td_add(&info, ']'); }
  Type_List tl = td->types;
  if(tl) {
    td_check(&info, 2);
    td_add(&info, '<');
    td_info_run(env, &info);
    td_add(&info, '>');
  }
  info.str[info.len - 1] = '\0';
  return info.str;
}

ANEW ANN m_str tl2str(const Env env, Type_List tl) {
  struct td_info info = { tl, (m_str)xmalloc(32), 1, 32 };
  td_info_run(env, &info);
  return info.str;
}

ANN static inline void* type_unknown(const ID_List id, const m_str orig) {
  char path[id_list_len(id)];
  type_path(path, id);
  ERR_O(id->pos, "'%s' unknown type in %s", path, orig);
}

ANN static Type prim_ref(const Type t, const Type_Decl* td) {
  if(GET_FLAG(td, ref) && isa(t, t_object) < 0)
    ERR_O(td->xid->pos, "primitive types cannot be used as reference (@)...\n")
  return t;
}

ANN Type known_type(const Env env, const Type_Decl* td, const m_str orig) {
  const Type t = type_decl_resolve(env, td);
  return t ? prim_ref(t, td) : type_unknown(td->xid, orig);
}
