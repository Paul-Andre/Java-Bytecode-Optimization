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

/* iload x
 * ldc k   (0<=k<=127)
 * iadd
 * istore x
 * --------->
 * iinc x k
 */ 
int positive_increment(CODE **c)
{ int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_iadd(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=127) {
     return replace(c,4,makeCODEiinc(x,k,NULL));
  }
  return 0;
}

/* goto L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * --------->
 * goto L2
 * ...
 * L1:    (reference count reduced by 1)
 * goto L2
 * ...
 * L2:    (reference count increased by 1)  
 */
int simplify_goto_goto(CODE **c)
{ int l1,l2;
  if (is_goto(*c,&l1) && is_goto(next(destination(l1)),&l2) && l1>l2) {
     droplabel(l1);
     copylabel(l2);
     return replace(c,1,makeCODEgoto(l2,NULL));
  }
  return 0;
}


/* Simplifies a chain of gotos, detecting loops.
 */
/*
int simplify_goto_chain(CODE **c) {
  int label;
  if (is_goto(*c, &label)) {
    int previously_seen[16];
    int index = 0;
    int current = &label;
    previously_seen[index] = current;
    index ++;
    while (true) {
      int next_label;
      if (is_goto(next(destination(current)), &next_label)) {
      }
      }
      }
      }
      */


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
 * dup
 * ifeq L1
 * pop
 * ...
 * L1:
 * ifeq L2
 * ...
 * L2:
 * --------->
 * ifeq L2
 * ...
 * L1: (reference count reduced by 1)
 * ifeq L2
 * ...
 * L2: (reference count increased by 1)
 */
int simplify_dup_ifeq_ifeq(CODE **c) {
  int l1, l2;
  if (is_dup(*c) &&
      is_ifeq(next(*c),&l1) &&
      is_pop(next(next(*c))) &&
      is_ifeq(next(destination(l1)),&l2) ) {
    droplabel(l1);
    copylabel(l2);
    return replace(c,3,makeCODEifeq(l2,NULL));
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
 * ldc [not 0]
 * ifeq L
 * ...
 * L:
 * --------->
 * [nothing]
 * ...
 * L: (reference counter reduced by 1)
 */

int remove_iconst_ifeq(CODE **c) {
  int v;
  int l;
  if (is_ldc_int(*c, &v) && v!=0 &&
      is_ifeq(next(*c),&l)) {
    droplabel(l);
    return replace(c, 2, NULL);
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

void init_patterns(void) {
  /*ADD_PATTERN(constant_fold);*/
  ADD_PATTERN(goto_return);
  ADD_PATTERN(invert_comparison);
	ADD_PATTERN(simplify_dup_xxx_pop);
	ADD_PATTERN(simplify_member_store);
	ADD_PATTERN(simplify_astore_aload);
	ADD_PATTERN(simplify_multiplication_right);
	ADD_PATTERN(positive_increment);
  ADD_PATTERN(simplify_iconst_0_goto_ifeq);
  ADD_PATTERN(simplify_dup_ifeq_ifeq);
	ADD_PATTERN(simplify_goto_goto);
  ADD_PATTERN(remove_iconst_ifeq);
	ADD_PATTERN(remove_dead_label);
	ADD_PATTERN(fuse_labels);
	ADD_PATTERN(remove_instruction_after_goto);
	ADD_PATTERN(remove_instruction_after_return);
}
