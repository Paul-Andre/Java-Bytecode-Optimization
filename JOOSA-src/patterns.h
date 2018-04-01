/*
 * JOOS is Copyright (C) 1997 Laurie Hendren & Michael I. Schwartzbach
 *
 * Reproduction of all or part of this software is permitted for
 * educational or research use on condition that this copyright notice is
 * included in any copy. This software comes with no warranty of any
 * kind. In no event will the authors be liable for any damages resulting from
 * use of this software.
 *
 * email: hendren@cs.mcgill.ca, mis@brics.dk
 */

int set_label(CODE *c, int l);

/* iload x        iload x        iload x
 * ldc 0          ldc 1          ldc 2
 * imul           imul           imul
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x
 *                               dup
 *                               iadd
 */

int simplify_multiplication_right(CODE **c)
{ int x,k;
  if (is_iload(*c,&x) && 
      is_ldc_int(next(*c),&k) && 
      is_imul(next(next(*c)))) {
     if (k==0) return replace(c,3,makeCODEldc_int(0,NULL));
     else if (k==1) return replace(c,3,makeCODEiload(x,NULL));
     else if (k==2) return replace(c,3,makeCODEiload(x,
                                       makeCODEdup(
                                       makeCODEiadd(NULL))));
     return 0;
  }
  return 0;
}

/* dup
 * astore x
 * pop
 * -------->
 * astore x
 */
int simplify_astore(CODE **c)
{ int x;
  if (is_dup(*c) &&
      is_astore(next(*c),&x) &&
      is_pop(next(next(*c)))) {
     return replace(c,3,makeCODEastore(x,NULL));
  }
  return 0;
}

/* dup
 * pop
 * -------->
 * [nothing]
 */
int dup_pop(CODE **c)
{
  if (is_dup(*c) &&
      is_pop(next(*c))) {
     return replace(c,2,NULL);
  }
  return 0;
}

/* ldc k   (0<=k<=127)
 * iadd
 * istore x
 * --------->
 * istore x
 * iinc x k
 */ 
/* XXX: this version can possibly increase code size */
int positive_increment(CODE **c)
{ int x,k;
  if (is_ldc_int(*c,&k) &&
      is_iadd(next(*c)) &&
      is_istore(next(next(*c)),&x) &&
      0<=k && k<=127) {
     return replace(c,3,makeCODEistore(x,makeCODEiinc(x,k,NULL)));
  }
  return 0;
}

/* ldc k   (0<=k<=127)
 * isub
 * istore x
 * --------->
 * istore x
 * iinc x -k
 */ 
/* XXX: this version can possibly increase code size */
int negative_increment(CODE **c)
{ int x,k;
  if (is_ldc_int(*c,&k) &&
      is_isub(next(*c)) &&
      is_istore(next(next(*c)),&x) &&
      0<=k && k<=127) {
     return replace(c,3,makeCODEistore(x,makeCODEiinc(x,-k,NULL)));
  }
  return 0;
}

/* goto/cmp L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * [not goto]
 * --------->
 * goto/cmp L2
 * ...
 * L1:    (reference count reduced by 1)
 * goto L2
 * ...
 * L2:    (reference count increased by 1)  
 * [not goto]
 *
 *
 * The not goto at the end is required to ensure termination in a case of
 * cyclical gotos
 */
int simplify_goto_goto(CODE **c)
{ int l1,l2;
  int dummy;
  if (uses_label(*c,&l1) &&
      is_goto(next(destination(l1)),&l2) &&
      !is_goto(next(destination(l2)),&dummy)) {
     droplabel(l1);
     copylabel(l2);
     set_label(*c,l2);
     /*return replace(c,1,makeCODEgoto(l2,NULL));*/
     return 1;
  }
  return 0;
}


/*
 * iconst_0
 * goto L1
 * ...
 * L1:
 * ifeq L2
 * ...
 * L2:
 * --------->
 * goto L2
 * ...
 * L1: (reference count reduced by 1)
 * ifeq L2
 * ...
 * L2: (reference count increased by 1)
 */
int simplify_iconst_0_goto_ifeq(CODE **c) {
  int v;
  int l1, l2;
  if (is_ldc_int(*c,&v) && v==0 &&
      is_goto(next(*c),&l1) &&
      is_ifeq(next(destination(l1)),&l2) ) {
    droplabel(l1);
    copylabel(l2);
    return replace(c,2,makeCODEgoto(l2,NULL));
  }
  return 0;
}

/*
 * iconst_0
 * goto L1
 * ...
 * L1:
 * dup
 * ifeq L2
 * ...
 * L2:
 * --------->
 * iconst 0
 * goto L2
 * ...
 * L1: (reference count reduced by 1)
 * dup
 * ifeq L2
 * ...
 * L2: (reference count increased by 1)
 */
int simplify_iconst_0_goto_dup_ifeq(CODE **c) {
  int v;
  int l1, l2;
  if (is_ldc_int(*c,&v) && v==0 &&
      is_goto(next(*c),&l1) &&
      is_dup(next(destination(l1))) &&
      is_ifeq(next(next(destination(l1))),&l2)
      ) {
    droplabel(l1);
    copylabel(l2);
    return replace(c,2,makeCODEldc_int(0, makeCODEgoto(l2,NULL)));
  }
  return 0;
}

/* iconst v (not 0)
 * dup
 * ifeq L1
 * pop
 * ...
 * L1:
 * ---------->
 * [nothing]
 * ...
 * L1: (reference count reduced by 1)
 */
int simplify_iconst_1_dup_ifeq_pop(CODE **c) {
  int v;
  int l;
  if (is_ldc_int(*c, &v) && v!=0 &&
      is_dup(next(*c)) &&
      is_ifeq(next(next(*c)), &l) &&
      is_pop(next(next(next(*c)))) ) {
    droplabel(l);
    return replace(c,4,NULL);
  }
  return 0;
}


/*
 * dup
 * ifeq/ifne L1
 * pop
 * ...
 * L1:
 * ifeq/ifne L2  (And same as first ifeq/ifneq)
 * ...
 * L2:
 * --------->
 * ifeq/ifne L2
 * ...
 * L1: (reference count reduced by 1)
 * ifeq/ifne L2
 * ...
 * L2: (reference count increased by 1)
 */
int simplify_dup_ifeq_ifeq(CODE **c) {
  int l1, l2;
  int d;
  int aeq, beq;
  if (is_dup(*c) &&
      ( is_ifeq(next(*c),&l1) || is_ifne(next(*c),&l1)) &&
      is_pop(next(next(*c))) &&
      (is_ifeq(next(destination(l1)),&l2) || is_ifne(next(destination(l1)),&l2))
      ) {
    aeq = is_ifeq(next(*c), &d);
    beq = is_ifeq(next(destination(l1)), &d);
    if (aeq == beq) {
      droplabel(l1);
      copylabel(l2);
      if (aeq) {
        return replace(c,3,makeCODEifeq(l2,NULL));
      }
      else {
        return replace(c,3,makeCODEifne(l2,NULL));
      }
    }
  }
  return 0;
}

/*
 * dup
 * ifeq/ifne L1
 * pop
 * ...
 * L1:
 * ifne/ifeq L2  (Different from first ifeq/ifne)
 * ...
 * L2:
 * --------->
 * ifeq/ifne L3
 * ...
 * L1: (reference count reduced by 1)
 * ifne/ifeq L2
 * L3: (new label)
 * ...
 * L2:
 */
int simplify_dup_ifeq_ifne(CODE **c) {
  int l1, l2, l3;
  int d;
  int aeq, beq;
  CODE *l3_code;
  if (is_dup(*c) &&
      ( is_ifeq(next(*c),&l1) || is_ifne(next(*c),&l1)) &&
      is_pop(next(next(*c))) &&
      (is_ifeq(next(destination(l1)),&l2) || is_ifne(next(destination(l1)),&l2))
      ) {
    aeq = is_ifeq(next(*c), &d);
    beq = is_ifeq(next(destination(l1)), &d);
    if (aeq != beq) {
      droplabel(l1);

      l3 = next_label();
      l3_code = makeCODElabel(l3, next(next(destination(l1))));
      INSERTnewlabel(l3, "asdf", l3_code, 1);
      next(destination(l1))->next = l3_code;

      if (aeq) {
        replace(c,3,makeCODEifeq(l3,NULL));
      }
      else {
        replace(c,3,makeCODEifne(l3,NULL));
      }

      return 1;
    }
  }
  return 0;
}


int is_boolcmp(CODE *c, int *v, int *l) {
  if (is_ifeq(c, l)) {
    *v = 0;
    return 1;
  } else if (is_ifne(c, l)) {
    *v = 1;
    return 1;
  }
  return 0;
}

CODE *makeCODEboolcmp(int v, int l, CODE *next) {
  if (v == 0) {
    return makeCODEifeq(l, next);
  }
  if (v == 1) {
    return makeCODEifne(l, next);
  }
  return NULL;
}


/* iconst_0/1
 * goto L1
 * ...
 * L1:
 * ifeq/ifne L2
 * ...
 * L2
 * ---------->
 * goto L2
 * ...
 * L1: (reference count reduced by 1)
 * ifeq/ifne L2
 * ...
 * L2: (reference count increased by 1)
 *
 * ==========================================
 *
 * iconst_0/1
 * goto L1
 * ...
 * L1:
 * ifne/ifeq L2
 * ...
 * L2:
 * ---------->
 * goto L3
 * ...
 * L1: (reference count reduced by 1)
 * ifne/ifeq L2
 * L3: (new label)
 * ...
 * L2: 
 */
int simplify_iconst_goto_ifeq(CODE **c) {
  int v1;
  int v2;
  int l1, l2;
  int l3;
  CODE *l3_code;
  if (is_ldc_int(*c, &v1) &&
      is_goto(next(*c), &l1) &&
      is_boolcmp(next(destination(l1)), &v2, &l2)) {
    if ((v1!=0) == v2) {
      droplabel(l1);
      copylabel(l2);
      return replace(c, 2, makeCODEgoto(l2, NULL));
    } else if ((v1!=0) != v2) {
      droplabel(l1);

      l3 = next_label();
      l3_code = makeCODElabel(l3, next(next(destination(l1))));
      INSERTnewlabel(l3, "simplified_iconst_goto_ifeq", l3_code, 1);
      next(destination(l1))->next = l3_code;

      return replace(c, 2, makeCODEgoto(l3, NULL));
    }
  }
  return 0;
}


int remove_dead_label(CODE **c) {
  int l;
  if (is_label(*c, &l) && deadlabel(l)) {
    return replace(c, 1, NULL);
  }
  return 0;
}


int set_label(CODE *c, int l) {
  switch (c->kind) {

    case gotoCK:
      c->val.gotoC = l;

    case ifeqCK:
      c->val.ifeqC = l;
      break;
    case ifneCK:
      c->val.ifneC = l;
      break;
    case if_acmpeqCK:
      c->val.if_acmpeqC = l;
      break;
    case if_acmpneCK:
      c->val.if_acmpneC = l;
      break;
    case ifnullCK:
      c->val.ifnullC = l;
      break;
    case ifnonnullCK:
      c->val.ifnonnullC = l;
      break;

    case if_icmpeqCK:
      c->val.if_icmpeqC = l;
      break;
    case if_icmpneCK:
      c->val.if_icmpneC = l;
      break;
    case if_icmpgtCK:
      c->val.if_icmpgtC = l;
      break;
    case if_icmpltCK:
      c->val.if_icmpltC = l;
      break;
    case if_icmpgeCK:
      c->val.if_icmpgeC = l;
      break;
    case if_icmpleCK:
      c->val.if_icmpleC = l;
      break;

    default:
      return 0;
      break;
  }
  return 1;
}


/* goto/cmp L1
 * ...
 * L1:
 * L2:
 * --------->
 * goto/cmp L2
 * ...
 * L1: (reference count reduced by 1)
 * L2: (reference count increased by 1)
 */
int fuse_labels(CODE **c) {
  int l1, l2;
  if (uses_label(*c, &l1) && is_label(next(destination(l1)), &l2)) {
    droplabel(l1);
    copylabel(l2);
    
    set_label(*c, l2);
    return 1;
  }
  return 0;
}



/*
 * ldc 0/[not 0]
 * ifeq/ifneq L
 * ...
 * L:
 * --------->
 * goto L
 * ...
 * L:
 *
 * ==================================
 * ldc 0/[not 0]
 * ifne/ifeq L
 * ...
 * L:
 * --------->
 * [nothing]
 * ...
 * L: (reference counter reduced by 1)
 */

int remove_iconst_ifeq(CODE **c) {
  int v;
  int b;
  int l;
  if (is_ldc_int(*c, &v) && 
      is_boolcmp(next(*c),&b,&l)) {
    if ((v!=0) == b) {
      return replace(c, 2, makeCODEgoto(l, NULL));
    }
    else if ((v!=0) != b) {
      droplabel(l);
      return replace(c, 2, NULL);
    }
  }
  return 0;
}



/*
 * dup
 * xxx [any instruction that only uses the top value and removes it]
 * pop
 * --------->
 * xxx
 */

int simplify_dup_xxx_pop(CODE **c) {
  if (is_dup(*c)) {
    int inc, affected, used;
    int code_type = stack_effect(next(*c), &inc, &affected, &used);
    if (used == -1 && inc == -1 &&
        code_type == 0 /* normal, ie not jump/conditional/label/break*/ ) {
      if (is_pop(next(next(*c)))) {
        next(*c)->next = next(next(next(*c)));
        *c = next(*c);
        return 1;
      }
    }
  }
  return 0;
}


/* dup
 * aload k (usually 0)
 * swap
 * putfield
 * pop
 * --------->
 * aload k
 * swap
 * putfield
 *
 * I believe this pattern occurs when storing to a member variable of the
 * current class
 */

int simplify_member_store(CODE **c) {
  int k;
  char *putfield_arg;
  if (is_dup(*c) &&
      is_aload(next(*c), &k) &&
      is_swap(next(next(*c))) &&
      is_putfield(next(next(next(*c))), &putfield_arg) &&
      is_pop(next(next(next(next(*c))))) ) {
    return 
      replace(c, 5,
      makeCODEaload(k, makeCODEswap( makeCODEputfield(putfield_arg, NULL))));
  }
  return 0;
}



/*
 * dup
 * pop
 * --------->
 * [nothing]
 */
int remove_dup_pop(CODE **c) {
  if (is_dup(*c) && is_pop(next(*c))) {
    return replace(c, 2, NULL);
  }
  return 0;
}

/*
 * astore k
 * aload k
 * --------->
 * dup
 * astore k
 */

int simplify_astore_aload(CODE **c) {
  int a, b;
  if (is_astore(*c, &a) && is_aload(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEdup(makeCODEastore(a, NULL)));
  }
  return 0;
}

/*
 * istore k
 * iload k
 * --------->
 * dup
 * istore k
 */

int simplify_istore_iload(CODE **c) {
  int a, b;
  if (is_istore(*c, &a) && is_iload(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEdup(makeCODEistore(a, NULL)));
  }
  return 0;
}

/* aload k
 * astore k
 * ---------->
 * [nothing]
 */
int simplify_aload_astore(CODE **c) {
  int a, b;
  if (is_aload(*c, &a) && is_astore(next(*c), &b) && a == b) {
    return replace(c, 2, NULL);
  }
  return 0;
}

/* iload k
 * istore k
 * ---------->
 * [nothing]
 */
int simplify_iload_istore(CODE **c) {
  int a, b;
  if (is_iload(*c, &a) && is_istore(next(*c), &b) && a == b) {
    return replace(c, 2, NULL);
  }
  return 0;
}


/*
 * if_comparison L1
 * goto L2
 * L1:
 * ..
 * L2:
 * --------->
 * if_not_comparison L2
 * L1: (reference counter reduced by 1)
 *
 * L2: (reference counter stays the same)
 */
int invert_comparison(CODE **c) {
  int l1;
  int l2, l3;
  if (*c == NULL) return 0;
  switch ((*c)->kind) {
    case ifeqCK:
      l1 = (*c)->val.ifeqC;
      break;
    case ifneCK:
      l1 = (*c)->val.ifneC;
      break;
    case if_acmpeqCK:
      l1 = (*c)->val.if_acmpeqC;
      break;
    case if_acmpneCK:
      l1 = (*c)->val.if_acmpneC;
      break;
    case ifnullCK:
      l1 = (*c)->val.ifnullC;
      break;
    case ifnonnullCK:
      l1 = (*c)->val.ifnonnullC;
      break;

    case if_icmpeqCK:
      l1 = (*c)->val.if_icmpeqC;
      break;
    case if_icmpneCK:
      l1 = (*c)->val.if_icmpneC;
      break;
    case if_icmpgtCK:
      l1 = (*c)->val.if_icmpgtC;
      break;
    case if_icmpltCK:
      l1 = (*c)->val.if_icmpltC;
      break;
    case if_icmpgeCK:
      l1 = (*c)->val.if_icmpgeC;
      break;
    case if_icmpleCK:
      l1 = (*c)->val.if_icmpleC;
      break;

    default:
      return 0;
      break;
  }

  if (is_goto(next(*c), &l2) && is_label(next(next(*c)), &l3) && l1==l3) {
    CODE *new_code;

    droplabel(l1);

    switch ((*c)->kind) {
      case ifeqCK:
        new_code = makeCODEifne(l2,NULL);
        break;
      case ifneCK:
        new_code = makeCODEifeq(l2,NULL);
        break;
      case if_acmpeqCK:
        new_code = makeCODEif_acmpne(l2,NULL);
        break;
      case if_acmpneCK:
        new_code = makeCODEif_acmpeq(l2,NULL);
        break;
      case ifnullCK:
        new_code = makeCODEifnonnull(l2,NULL);
        break;
      case ifnonnullCK:
        new_code = makeCODEifnull(l2,NULL);
        break;

      case if_icmpeqCK:
        new_code = makeCODEif_icmpne(l2,NULL);
        break;
      case if_icmpneCK:
        new_code = makeCODEif_icmpeq(l2,NULL);
        break;
      case if_icmpgtCK:
        new_code = makeCODEif_icmple(l2,NULL);
        break;
      case if_icmpltCK:
        new_code = makeCODEif_icmpge(l2,NULL);
        break;
      case if_icmpgeCK:
        new_code = makeCODEif_icmplt(l2,NULL);
        break;
      case if_icmpleCK:
        new_code = makeCODEif_icmpgt(l2,NULL);
        break;
      default:
        printf("This shouldn't happen. (*c)->kind mysteriously changed.\n");
        return 0;
    }
    return replace(c, 2, new_code);
  }
  return 0;
}


int goto_return(CODE **c) {
  int l1;
  if(is_goto(*c, &l1) && is_return(next(destination(l1)))) {
    droplabel(l1);
    return replace(c, 1, makeCODEreturn(NULL));
  }
  return 0;
}

int remove_instruction_after_goto(CODE **c) {
  int l;
  int dummy;
  if (is_goto(*c, &l) && next(*c) != NULL &&
      /*!is_nop(next(*c)) &&*/
      !is_label(next(*c), &dummy)) {
    return replace_modified(&((*c)->next), 1, NULL);
  }
  return 0;
}

int remove_instruction_after_return(CODE **c) {
  int dummy;
  if ((is_areturn(*c) || is_ireturn(*c) || is_return(*c)) &&
      next(*c) != NULL &&
      /*!is_nop(next(*c)) &&*/
      !is_label(next(*c), &dummy)) {
    return replace_modified(&((*c)->next), 1, NULL);
  }
  return 0;
}

int constant_fold(CODE **c) {
  int a, b;
  if (is_ldc_int(*c, &a) && is_ldc_int(next(*c), &b)) {
    if (is_iadd(next(next(*c)))) {
      return replace(c, 3, makeCODEldc_int(a+b, NULL));
    } else if (is_imul(next(next(*c)))) {
      return replace(c, 3, makeCODEldc_int(a*b, NULL));
    }
  }
  return 0;
}

/* iconst_0
 * if_icmpeq/if_icmpne L1
 * ---------->
 * ifeq/ifne L1
 */
int simplify_icmp_0(CODE **c) {
  int v;
  int l;
  if (is_ldc_int(*c, &v) && v == 0) {
    if (is_if_icmpeq(next(*c), &l)) {
      return replace(c, 2, makeCODEifeq(l, NULL));
    } else if (is_if_icmpne(next(*c), &l)) {
      return replace(c, 2, makeCODEifne(l, NULL));
    }
  }
  return 0;
}

/* aconst_null
 * if_acmpeq/if_acmpne L1
 * ---------->
 * ifnull/ifnonnull L1
 */
int simplify_acmp_null(CODE **c) {
  int l;
  if (is_aconst_null(*c)) {
    if (is_if_acmpeq(next(*c), &l)) {
      return replace(c, 2, makeCODEifnull(l, NULL));
    } else if (is_if_acmpne(next(*c), &l)) {
      return replace(c, 2, makeCODEifnonnull(l, NULL));
    }
  }
  return 0;
}


/* Some words I wish to use throughout this code
 * expression: does not use the stack beneath it and pushes a single value
 * pure: no side effect
 * instruction: a single instruction
 * */
int is_pure_expression_instruction(CODE *c) {
  int d;
  char *d2;
  return (is_ldc_int(c, &d) || is_ldc_string(c,&d2) || is_aconst_null(c) ||
      is_aload(c, &d) || is_iload(c, &d));
}

/* ldc/load #1
 * ldc/load #2
 * swp
 * ---------->
 * #2
 * #1
 * swp
 */
int basic_unswap(CODE **c) {
  CODE *c1;
  CODE *c2;
  CODE *swap;
  if (is_pure_expression_instruction(*c) &&
      is_pure_expression_instruction(next(*c)) &&
      is_swap(next(next(*c)))) {
    c1 = *c;
    c2 = next(c1);
    swap = next(c2);
    *c = c2;
    c2->next = c1;
    c1->next = swap->next;
    return 1;
  }
  return 0;
}

#define N_LOOKAHEAD 32

/* Navigates the code to check that the variable is not loaded
 *
 * return 1 = no loads
 * return 0 = cannot say that there's no loads
 */
int check_no_loads(CODE *c, int k, int *count) {
  int var;
  int l;
  (*count)--;
  if (*count == 0) return 0;
  if ((is_iload(c, &var)||is_aload(c,&var)) && var == k) return 0;

  if (c == NULL) return 1;
  if (is_return(c) || is_ireturn(c) || is_areturn(c)) return 1;
  if ((is_istore(c, &var) || is_astore(c,&var)) && var == k) return 1; /* we ignore old value */

  if (is_goto(c, &l)) return check_no_loads(destination(l), k, count);

  if (uses_label(c, &l)) { /* is comparison */
    return check_no_loads(next(c), k, count) &&
      check_no_loads(destination(l), k, count);
  }
  return check_no_loads(next(c), k, count);
}

/* 
 * If the current operation is a store, it searches through a fixed set of
 * paths to see if it is ever loaded. If all the branches terminate or have the
 * variable stored again, then we know the current load will never be used.
 */
int remove_dead_store(CODE **c) {
  int k;
  int instruction_count = N_LOOKAHEAD;
  if ((is_istore(*c, &k) || is_astore(*c, &k))
      && check_no_loads(next(*c), k, &instruction_count)) {
    return replace(c, 1, makeCODEpop(NULL));
  }
  return 0;
}

/* ldc "asdfasdf"
 * dup
 * ifnonnull L1
 * ---------->
 * ldc "asdfasdf"
 * goto L1
 */

int simplify_ldc_string_ifnonnull(CODE **c) {
  char *s;
  int l;
  if (is_ldc_string(*c, &s) && is_dup(next(*c)) && is_ifnonnull(next(next(*c)), &l)){
    return replace(c, 3, makeCODEldc_string(s, makeCODEgoto(l, NULL)));
  }
  return 0;
}

/* invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
 * dup
 * ifnonnull L1
 * ---------->
 * invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;
 * goto L1
 */

int simplify_concat_string_ifnonnull(CODE **c) {
  char *s;
  int l;
  if (is_invokevirtual(*c, &s) &&
      strcmp(s,"java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;")==0 &&
      is_dup(next(*c)) &&
      is_ifnonnull(next(next(*c)), &l)){
    return replace(&((*c)->next), 2, makeCODEgoto(l, NULL));
  }
  return 0;
}

/* goto L1
 * L1:
 * ---------->
 * L1: (reference count reduced by 1)
 */
int remove_unnecessary_goto(CODE **c) {
  int l1, l2;
  if (is_goto(*c, &l1) && is_label(next(*c), &l2) && l1 == l2) {
    return replace_modified(c, 1, NULL);
  }
  return 0;
}


/* pure_expression_instruction
 * pop
 * ---------->
 * [nothing]
 */
int basic_expression_pop(CODE **c) {
  if (is_pure_expression_instruction(*c) &&
      is_pop(next(*c))) {
    return replace(c, 2, NULL);
  }
  return 0;
}



int check_and_compare(int f(CODE *), CODE *a, CODE *b) {
  return (f(a) && f(b));
}

int check_and_compare_int(int f(CODE *,int *), CODE *a, CODE *b) {
  int x, y;
  return (f(a, &x) && f(b, &y) && x==y);
}

int check_and_compare_string(int f(CODE *,char **), CODE *a, CODE *b) {
  char *x, *y;
  return (f(a, &x) && f(b, &y) && strcmp(x, y)==0 );
}

/* This is horrible. I was probably tired when I did this */
int instructions_equal(CODE *a, CODE *b) {
  int xa,ya,xb,yb;
  if (a->kind != b->kind) return 0;
  return 
    check_and_compare(is_nop, a, b) ||
    check_and_compare(is_i2c, a, b) ||
    check_and_compare(is_imul, a, b) ||
    check_and_compare(is_ineg, a, b) ||
    check_and_compare(is_irem, a, b) ||
    check_and_compare(is_isub, a, b) ||
    check_and_compare(is_idiv, a, b) ||
    check_and_compare(is_iadd, a, b) ||
    check_and_compare(is_ireturn, a, b) ||
    check_and_compare(is_areturn, a, b) ||
    check_and_compare(is_return, a, b) ||
    check_and_compare(is_dup, a, b) ||
    check_and_compare(is_pop, a, b) ||
    check_and_compare(is_swap, a, b) ||

    check_and_compare_int(is_label, a, b) ||
    check_and_compare_int(is_goto, a, b) ||
    check_and_compare_int(is_ifeq, a, b) ||
    check_and_compare_int(is_ifne, a, b) ||
    check_and_compare_int(is_if_acmpeq, a, b) ||
    check_and_compare_int(is_if_acmpne, a, b) ||
    check_and_compare_int(is_ifnull, a, b) ||
    check_and_compare_int(is_ifnonnull, a, b) ||
    check_and_compare_int(is_if_icmpeq, a, b) ||
    check_and_compare_int(is_if_icmpgt, a, b) ||
    check_and_compare_int(is_if_icmplt, a, b) ||
    check_and_compare_int(is_if_icmple, a, b) ||
    check_and_compare_int(is_if_icmpge, a, b) ||
    check_and_compare_int(is_if_icmpne, a, b) ||
    check_and_compare_int(is_aload, a, b) ||
    check_and_compare_int(is_astore, a, b) ||
    check_and_compare_int(is_iload, a, b) ||
    check_and_compare_int(is_istore, a, b) ||
    check_and_compare_int(is_ldc_int, a, b) ||

    check_and_compare_string(is_new, a, b) ||
    check_and_compare_string(is_instanceof, a, b) ||
    check_and_compare_string(is_checkcast, a, b) ||
    check_and_compare_string(is_ldc_string, a, b) ||
    check_and_compare_string(is_getfield, a, b) ||
    check_and_compare_string(is_putfield, a, b) ||
    check_and_compare_string(is_invokevirtual, a, b) ||
    check_and_compare_string(is_invokenonvirtual, a, b) ||

    (is_iinc(a, &xa, &ya) && is_iinc(b, &xb, &yb) && xa==ya && xb==yb)
    ;
}


/// XXX: this is not sound.
/// The main reason it is not sound is that a single JVM instruction can be
/// executed on different possible types. Because of that, merging two branches
/// might make the JVM verification fail.
/* instruction
 * goto L1
 * ...
 * same instruction
 * goto L1 / L1:
 * ...
 * L1:
 * --------->
 * goto L3
 * ...
 * L3:(new label)
 * same instruction
 * goto L1 / L1:
 * ...
 * L1 (reference count reduced by 1)
 */
int factor_instruction(CODE **c) {
  CODE *p, *prev;
  /*int count = 64;*/
  int l1, l2, l3;
  CODE *l3_code;
  int d; /*dummy*/
  if ((!is_label(*c,&d)) &&
        is_goto(next(*c), &l1)){
    prev = next(*c);
    p = next(next(*c));
    while(p!=NULL) {
      if (instructions_equal(*c, p) &&
          (is_goto(next(p), &l2) || is_label(next(p),&l2)) && l1==l2 ) {
        printf("%d %d \n",l1,l2);
        l3 = next_label();
        l3_code = makeCODElabel(l3, p);
        prev->next = l3_code;
        INSERTnewlabel(l3,"factoring",l3_code,1);
        droplabel(l1);
        return replace(c, 2, makeCODEgoto(l3, NULL));
      }
      prev = p;
      p = next(p);
    }
  }
  return 0;
}



void init_patterns(void) {
  /*ADD_PATTERN(constant_fold);*/
  ADD_PATTERN(goto_return);
  ADD_PATTERN(invert_comparison);
	ADD_PATTERN(simplify_dup_xxx_pop);
	ADD_PATTERN(simplify_member_store);
	ADD_PATTERN(simplify_astore_aload);
	ADD_PATTERN(simplify_istore_iload);
	ADD_PATTERN(simplify_multiplication_right);
	ADD_PATTERN(positive_increment);
  ADD_PATTERN(simplify_iconst_0_goto_ifeq);
	ADD_PATTERN(simplify_goto_goto);
  ADD_PATTERN(remove_iconst_ifeq);
	ADD_PATTERN(remove_dead_label);
	ADD_PATTERN(fuse_labels);
	ADD_PATTERN(remove_instruction_after_goto);
	ADD_PATTERN(remove_instruction_after_return);
	ADD_PATTERN(simplify_icmp_0);
	ADD_PATTERN(simplify_acmp_null);
	ADD_PATTERN(basic_unswap);
	ADD_PATTERN(dup_pop);
  ADD_PATTERN(simplify_ldc_string_ifnonnull);
  ADD_PATTERN(remove_unnecessary_goto);
  ADD_PATTERN(simplify_concat_string_ifnonnull);
	ADD_PATTERN(remove_dead_store);
  ADD_PATTERN(basic_expression_pop);
  ADD_PATTERN(simplify_dup_ifeq_ifeq);
  ADD_PATTERN(simplify_dup_ifeq_ifne);
  ADD_PATTERN(simplify_iconst_goto_ifeq);
  ADD_PATTERN(simplify_iconst_0_goto_dup_ifeq);
  ADD_PATTERN(simplify_iconst_1_dup_ifeq_pop);
  ADD_PATTERN(negative_increment);
	ADD_PATTERN(simplify_aload_astore);
	ADD_PATTERN(simplify_iload_istore);
	/*ADD_PATTERN(factor_instruction);*/
}
