#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "instr.h"
#include "object.h"
#include "emit.h"
#include "gwion.h"
#include "operator.h"
#include "import.h"
#include "gwi.h"
#include "specialid.h"
#include "gack.h"

static GACK(gack_none) {
  INTERP_PRINTF("None")
}

static OP_CHECK(opck_none) {
  Exp_Binary *bin = (Exp_Binary*)data;
  CHECK_NN(opck_rassign(env, data, mut))
  exp_setvar(bin->rhs, 1);
  return bin->rhs->info->type;
}

static OP_EMIT(opem_none) {
  const Instr instr = emit_add_instr(emit, RegPop);
  instr->m_val = SZ_INT;
  return GW_OK;
}

static const f_instr dotmember[]  = { DotMember, DotMember2, DotMember3, DotMember4 };

ANN Instr emit_kind(Emitter emit, const m_uint size, const uint addr, const f_instr func[]);

static OP_EMIT(opem_union_dot) {
  const Exp_Dot *member = (Exp_Dot*)data;
  const Map map = &member->t_base->nspc->info->value->map;
  CHECK_BB(emit_exp(emit, member->base))
  if(isa(exp_self(member)->info->type, emit->gwion->type[et_function]) > 0) {
    const Instr instr = emit_add_instr(emit, RegPushImm);
    const Func f = (Func)vector_front(&member->t_base->info->parent->nspc->info->vtable);
    instr->m_val = (m_uint)f->code;
    return GW_OK;
  }
  for(m_uint i = 0; i < map_size(map); ++i) {
    if(VKEY(map, i) == (m_uint)member->xid) {
      const Value v = (Value)VVAL(map, i);
      const uint emit_addr = exp_getvar(exp_self(member));
      const Instr pre = emit_add_instr(emit,
        !emit_addr ? UnionCheck : UnionSet);
      pre->m_val = i + 1;
      const Instr instr = emit_kind(emit, v->type->size, emit_addr, dotmember);
      instr->m_val = SZ_INT;
      instr->m_val2 = v->type->size;
      return GW_OK;
    }
  }
  return GW_ERROR;
}

static DTOR(UnionDtor) {
  const m_uint idx = *(m_uint*)o->data;
  if(idx) {
    const Map map = &o->type_ref->nspc->info->value->map;
    const Value v = (Value)map_at(map, idx-1);
    if(isa(v->type, shred->info->vm->gwion->type[et_compound]) > 0)
      compound_release(shred, v->type, (o->data + SZ_INT));
  }
}

static OP_CHECK(opck_union_is) {
  const Exp e = (Exp)data;
  const Exp_Call *call = &e->d.exp_call;
  const Exp exp = call->args;
  if(exp->exp_type != ae_exp_primary && exp->d.prim.prim_type != ae_prim_id)
    ERR_N(exp->pos, "Union.is() argument must be of form id");
  const Type t = call->func->d.exp_dot.t_base;
  const Value v = find_value(t, exp->d.prim.d.var);
  if(!v)
    ERR_N(exp->pos, "'%s' has no member '%s'", t->name, s_name(exp->d.prim.d.var));
  const Map map = &t->nspc->info->value->map;
  for(m_uint i = 0; i < map_size(map); ++i) {
    const Value v = (Value)VVAL(map, i);
    if(!strcmp(s_name(exp->d.prim.d.var), v->name)) {
      exp->d.prim.prim_type = ae_prim_num;
      exp->d.prim.d.num = i+1;
      return env->gwion->type[et_bool];
    }
  }
  return env->gwion->type[et_error];
}

static MFUN(union_is) {
  *(m_uint*)RETURN = *(m_uint*)MEM(SZ_INT) == *(m_uint*)o->data;
}

ANN GWION_IMPORT(union) {
  const Type t_none = gwi_mk_type(gwi, "None", 0, NULL);
  GWI_BB(gwi_set_global_type(gwi, t_none, et_none))
  GWI_BB(gwi_gack(gwi, t_none, gack_none))
  gwi_add_type(gwi, t_none);
  struct SpecialId_ spid = { .type=gwi->gwion->type[et_none], .exec=NoOp, .is_const=1 };
  gwi_specialid(gwi, "None", &spid);

  GWI_BB(gwi_oper_ini(gwi, "None", "None", "None"))
  GWI_BB(gwi_oper_add(gwi, opck_none))
  GWI_BB(gwi_oper_emi(gwi, opem_none))
  GWI_BB(gwi_oper_end(gwi, "=>", NoOp))

  const Type t_union = gwi_class_ini(gwi, "@Union", "Object");
  gwi_class_xtor(gwi, NULL, UnionDtor);
  GWI_BB(gwi_item_ini(gwi, "int", "@index"))
  GWI_BB(gwi_item_end(gwi, ae_flag_none, NULL))
  GWI_BB(gwi_func_ini(gwi, "bool", "is"))
  GWI_BB(gwi_func_arg(gwi, "int", "member"))
  GWI_BB(gwi_func_end(gwi, union_is, ae_flag_none))
  GWI_BB(gwi_class_end(gwi))
  const Func f = (Func)vector_front(&t_union->nspc->info->vtable);
  const struct Op_Func opfunc = { .ck=opck_union_is };
  const struct Op_Import opi = { .rhs=f->value_ref->type,
       .func=&opfunc, .data=(uintptr_t)f, .pos=gwi->loc, .op=insert_symbol(gwi->gwion->st, "@func_check") };
  CHECK_BB(add_op(gwi->gwion, &opi))
  gwi->gwion->type[et_union] = t_union;

  GWI_BB(gwi_oper_ini(gwi, "@Union", (m_str)OP_ANY_TYPE, NULL))
  GWI_BB(gwi_oper_emi(gwi, opem_union_dot))
  GWI_BB(gwi_oper_end(gwi, "@dot", NULL))

  GWI_BB(gwi_union_ini(gwi, "Option:[A]"))
  GWI_BB(gwi_union_add(gwi, "None", "none"))
  GWI_BB(gwi_union_add(gwi, "A", "val"))
  GWI_BB(gwi_union_end(gwi, ae_flag_none))

  return GW_OK;
}
