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


/* Every pattern lexicographically reduces the tuple consisting of the
 * following things: (in decreasing order of priority)
 *
 * Byte code size
 * Number of jumps to a [goto] or a [dup; ifeq] sequence
 * Number of loads
 * Number of multiplication
 * Number of labels
 * Number of jumps that don't use the lowest label when an instruction has multiple labels
 *
 */

int is_goto_or_dup_ifeq(CODE *c) {
    int dummy;
    return is_goto(c, &dummy) || (is_dup(c) && is_ifeq(next(c), &dummy));
}

int set_label(CODE *c, int l);

/* iload x        iload x        iload x (1-2 bytes)
 * ldc 0          ldc 1          ldc 2   (1 byte)
 * imul           imul           imul    (1 byte
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x (1-2 bytes)
 *                               dup     (1 byte)
 *                               iadd    (1 byte)
 *
 * Improvement:
 *      First two changes reduce bytecode count.
 *      Third change keeps all measures the same but reduces the number of multiplications.
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

/* [before]         [ a * ]
 * dup              [ a a ]
 * astore x         [ a * ]     a is stored at x
 * pop              [ * * ]
 * -------->
 * [before]         [ a * ]
 * astore x         [ * * ]     a is stored at x
 *
 *
 * Duplicated value is unused and popped later on
 *
 * Improvement:
 *      Reduces bytecode size.
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

/* [before]         [ a * ]
 * dup              [ a a ]
 * pop              [ a * ]
 * -------->
 * [before]         [ a * ]
 * [nothing]        [ a * ]
 *
 * Duplicated value is popped right away
 *
 * Improvement:
 *      Reduces bytecode count
 *
 * TODO:
 */
int dup_pop(CODE **c)
{
  if (is_dup(*c) &&
      is_pop(next(*c))) {
     return replace(c,2,NULL);
  }
  return 0;
}

/* [before]                 [ a * ]
 * ldc k   (0<=k<=127)      [ a   k ]1-2
 * iadd                     [ a+k * ]1
 * istore x                 [ *   * ]1-2     a+k is stored at x
 * --------->
 * [before]                 [ a * ]
 * istore x                 [ * * ]1-2       a is stored at x
 * iinc x k                 [ * * ]3       a+k is stored at x
 */ 
/* XXX: this version can possibly increase code size in terms of actual bytecode size */
/*
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
*/

/* template version (turns out to be better)
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

/* ldc k   (0<=k<=127)
 * isub
 * istore x
 * --------->
 * istore x
 * iinc x -k
 */ 
/* XXX: this version can possibly increase code size */
/*
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
*/
/* template version 
 */
/* [before]     [ *   * ]     a is at location x
 * iload x      [ a   * ]
 * ldc k        [ a   k ]
 * isub         [ a-k * ]
 * istore x     [ *   * ]       a-k stored at x
 * --------->
 * [before]     [ *   * ]
 * iinc x -k    [ *   * ]       a-k stored at x
 */
int negative_increment(CODE **c)
{ int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_isub(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=127) {
     return replace(c,4,makeCODEiinc(x,-k,NULL));
  }
  return 0;
}


/* goto/cmp L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * [not goto or dup;ifeq sequence]
 * --------->
 * goto/cmp L2
 * ...
 * L1:    (reference count reduced by 1)|not visited anymore
 * goto L2                              |not visited anymore
 * ...
 * L2:    (reference count increased by 1)  
 * [not goto or dup;ifeq sequence]
 *
 * We go to L1, then directly to L2. So it is safe to go directly to L2.
 * The not goto at the end is required to ensure termination in a case of
 * cyclical gotos
 *
 * TODO: might end up cycling
 *
 * Improvements: 
 *  - Keeps bytecode size the same
 *  - Decreases the number of jumps towards a goto
 */
int simplify_goto_goto(CODE **c)
{ int l1,l2;
  if (uses_label(*c,&l1) &&
      is_goto(next(destination(l1)),&l2) &&
      !is_goto_or_dup_ifeq(next(destination(l2)))) {
     droplabel(l1);
     copylabel(l2);
     set_label(*c,l2);
     /*return replace(c,1,makeCODEgoto(l2,NULL));*/
     return 1;
  }
  return 0;
}


/*
 * iconst_0         [ 0 ]
 * goto L1          [ 0 ]
 * ...
 * L1:              [ 0 ]
 * ifeq L2          [ * ]   Jump to L2
 * ...
 * L2:              [ * ]
 * --------->
 * goto L2          [ * ]
 * ...
 * L1:                      (reference count reduced by 1) |not visited anymore
 * ifeq L2                                                 |not visited anymore
 * ...
 * L2:              [ * ]   (reference count increased by 1)
 *
 * 
 * When we reach the L1 label, 0 is on the stack, so we will always jump to L2.
 * It is safe to jump to L2 directly
 *
 * Improvements: 
 *  - Reduces becode size
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
 * iconst_0         [ 0 * ]
 * goto L1          [ 0 * ]
 * ...
 * L1:              [ 0 * ]
 * dup              [ 0 0 ]
 * ifeq L2          [ 0 * ]     Jump to L2
 * ...
 * L2:              [ 0 * ]
 * [not goto or dup;ifeq sequence]
 * --------->
 * iconst 0         [ 0 * ]
 * goto L2          [ 0 * ]
 * ...
 * L1:                          (reference count reduced by 1)|not visited anymore
 * dup                                                        |not visited anymore
 * ifeq L2                                                    |not visited anymore
 * ...
 * L2:              [ 0 * ]     (reference count increased by 1)
 * [not goto or dup;ifeq sequence]
 *
 *
 * Improvement:
 *      decreases the number of jumps to [goto] or [dup; ifeq] while keeping everything else the same
 *
 */
int simplify_iconst_0_goto_dup_ifeq(CODE **c) {
  int v;
  int l1, l2;
  if (is_ldc_int(*c,&v) && v==0 &&
      is_goto(next(*c),&l1) &&
      is_dup(next(destination(l1))) &&
      is_ifeq(next(next(destination(l1))),&l2) &&
      !is_goto_or_dup_ifeq(destination(l2))
      ) {
    droplabel(l1);
    copylabel(l2);
    return replace(c,2,makeCODEldc_int(0, makeCODEgoto(l2,NULL)));
  }
  return 0;
}

/* iconst v (v != 0)        [ v * ]
 * dup                      [ v v ]
 * ifeq L1                  [ v * ]     Never jumps since v!=0
 * pop                      [ * * ]
 * ...
 * L1:                                  | Not visited anymore in the context of the current pattern
 * ---------->
 * [nothing]                [ * * ]
 * ...
 * L1: (reference count reduced by 1)
 *
 * Improvement:
 *      Reduces size of bytecode
 */
int simplify_iconst_1_dup_ifeq_pop(CODE **c) {
  int v;
  int l;
  if (is_ldc_int(*c, &v) && v!=0 &&
      is_dup(next(*c)) &&
      is_ifeq(next(next(*c)), &l) &&
      is_pop(next(next(next(*c)))) ) {
    droplabel(l);
    return replace(c,4,makeCODEnop(NULL));
  }
  return 0;
}


/*
 *  [before]                        [ a * ]
 * dup                              [ a a ]
 * ifeq/ifne L1                     [ a * ]     Possibly going to L1
 * pop                              [ * * ]
 * ...
 * L1:                              [ a * ]  
 * ifeq/ifne L2 (same check)        [ * * ]     If this is exactly the same check the outcome will be the same when applied to a.
 *                                              If we got here by jumping from the first comparison, we would also do this second
 *                                              jump and go to L2
 * ...
 * L2:                              [ * * ]
 * --------->
 *
 *  [before]                        [ a * ]
 * ifeq/ifne L2                     [ * * ]
 * ...
 * L1: (ref count -- )                      
 * ifeq/ifne L2                             
 * ...
 * L2: (ref count ++ )              [ * * ]
 *
 *
 * Improvement:
 *      Reduces size of bytecode
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
 * [before]                         [ a * ]
 * dup                              [ a a ]
 * ifeq/ifne L1                     [ a * ]     Possibly going to L1
 * pop                              [ * * ]
 * ...
 * L1:                              [ a * ]  
 * ifne/ifeq L2  (Opposite check)   [ * * ]     If this is a check opposite to the first one, then if we reached this instruction
 *                                              by jumping from the first check, then this will be false.
 * ...
 * L2:
 * --------->
 *
 *  [before]                        [ a ]
 * ifeq/ifne L3                     [ * ]
 * ...
 * L1: (refercount reduced by 1)        | Not reachable in the context of the current pattern
 * ifne/ifeq L2                         | Not reachable in the context of the current pattern
 * L3: (new label)                  [ * ]
 * ...
 * L2:
 *
 *
 * Improvement:
 *      Reduces size of bytecode
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
      INSERTnewlabel(l3, "jump_over", l3_code, 1);
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


/* Checks if an instruction is a "boolean comparison", ie ifeq or ifne */
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

/* Creates a "boolean comparison" instruction */
CODE *makeCODEboolcmp(int v, int l, CODE *next) {
  if (v == 0) {
    return makeCODEifeq(l, next);
  }
  if (v != 1) {
    return makeCODEifne(l, next);
  }
  return NULL;
}


/* iconst_0/1               [ 0 ] or [ 1 ]
 * goto L1                  [ 0 ] or [ 1 ]
 * ...
 * L1:                      [ 0 ] or [ 1 ]
 * ifeq/ifne L2             [ * ]           Jump to L2 in either case
 * ...
 * L2                       [ * ]
 * ---------->
 * goto L2                  [ * ]
 * ...
 * L1: (reference count reduced by 1) | Not reached 
 * ifeq/ifne L2                       | Not reached
 * ...
 * L2: (reference count increased by 1) [ * ]
 *
 * ==========================================
 *
 * iconst_0/1               [ 0 ] or [ 1 ]
 * goto L1                  [ 0 ] or [ 1 ]
 * ...
 * L1:                      [ 0 ] or [ 1 ]
 * ifne/ifeq L2             [ * ]           No jump to L2 in either case
 * ...
 * L2:                      
 * ---------->
 * goto L3                  [ * ]
 * ...
 * L1:                                      (reference count reduced by 1) | Not reached
 * ifne/ifeq L2                                                            | Not reached
 * L3:                      [ * ]           (new label)
 * ...
 * L2: 
 *
 *
 * Improvement:
 *      Reduces bytecode size
 * 
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


/*
 * Deletes a label that has reference count zero.
 * We never increment the reference count of a label that has no goto pointing to it.
 *
 * Improvement:
 *      Reduces the number of labels while keeping everything else the same
 */
int remove_dead_label(CODE **c) {
  int l;
  if (is_label(*c, &l) && deadlabel(l)) {
    return replace(c, 1, NULL);
  }
  return 0;
}

/* Helper to change the label of a jump */
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
 * L1: (reference count reduced by 1)       | Not reached in the context of current pattern
 * L2: (reference count increased by 1)
 *
 *
 * Improvement:
 *      After the peephole runs this multiple times, there will be one jump
 *      less that doesn't use the lowest label for a given instruction.
 *      Everything else stays the same
 *
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
 * ldc 0/[not 0]            [ 0 ] or [ k ] (k != 0)
 * ifeq/ifneq L             [ * ]          Jump to L in either case
 * ...
 * L:                       [ * ]
 * --------->
 * goto L                   [ * ]
 * ...
 * L:                       [ * ]
 *
 * ==================================
 * ldc 0/[not 0]            [ 0 ] or [ k ] (k != 0)
 * ifne/ifeq L              [ * ]          No jump in either case
 * ...
 * L:
 * --------->
 * nop                      [ * ]
 * ...
 * L:                                      (reference counter reduced by 1)
 *
 *
 * Improvement:
 *      Reduces bytecode size
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
      return replace(c, 2, makeCODEnop(NULL));
    }
  }
  return 0;
}



/* [before]         [ ... k * ]
 * dup              [ ... k k ]
 * xxx              [ ... k * ] (any instruction that only uses the top value and removes it)
 * pop              [ ... * * ]
 * --------->
 * [before]         [ ... k * ]
 * xxx              [ ... * * ]
 *
 * Improvement:
 *      Reduces bytecode size
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


/* [before]             [ c * * ]
 * dup                  [ c c * ]
 * aload k              [ c c k ] (k usually 0 which is the "this" object)
 * swap                 [ c k c ]
 * putfield             [ c * * ]
 * pop                  [ * * * ]
 * --------->
 * aload k              [ c k * ]
 * swap                 [ k c * ]
 * putfield             [ * * * ]
 *
 * I believe this pattern occurs when storing to a member variable of the
 * current class
 *
 * Improvement:
 *      Reduces bytecode size
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



/* [before]     [ k * ]
 * dup          [ k k ]
 * pop          [ k * ]
 * --------->
 * nop          [ k * ]
 *
 * Improvement:
 *      Reduces bytecode size
 */
int remove_dup_pop(CODE **c) {
  if (is_dup(*c) && is_pop(next(*c))) {
    return replace(c, 2, makeCODEnop(NULL));
  }
  return 0;
}

/* [before]     [ a ]
 * astore k     [ * ]   (1-2 bytes) a gets stored in k
 * aload k      [ a ]   (1-2 bytes)
 * --------->
 * dup          [ a a ] (1 byte)
 * astore k     [ a * ] (1-2 bytes)
 *
 * In both cases we end up storing i in k 
 *
 * Improvement:
 *      It doesn't increase bytecode size and reduces number of loads
 */

int simplify_astore_aload(CODE **c) {
  int a, b;
  if (is_astore(*c, &a) && is_aload(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEdup(makeCODEastore(a, NULL)));
  }
  return 0;
}

/* [before]     [ i ]
 * istore k     [ * ]   (1-2 bytes)     i gets stored in k
 * iload k      [ i ]   (1-2 bytes)
 * --------->
 * dup          [ i i ] (1 bytes)
 * istore k     [ i * ] (1-2 bytes)
 *
 * In both cases we end up storing i in k 
 *
 * Improvement:
 *      It doesn't increase bytecode size and reduces number of loads
 */

int simplify_istore_iload(CODE **c) {
  int a, b;
  if (is_istore(*c, &a) && is_iload(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEdup(makeCODEistore(a, NULL)));
  }
  return 0;
}

/* aload k      [ a ]
 * astore k     [ * ]
 * ---------->
 * nop          [ * ]
 *
 * Since we store back the value we loaded from k, it essentially does nothin
 *
 * Improvement:
 *      Reduces bytecode size
 */
int simplify_aload_astore(CODE **c) {
  int a, b;
  if (is_aload(*c, &a) && is_astore(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEnop(NULL));
  }
  return 0;
}

/* iload k      [ a ]
 * istore k     [ * ]
 * ---------->
 * nop          [ * ]
 *
 * Since we store back the value we loaded from k, it essentially does nothin
 *
 * Improvement:
 *      Reduces bytecode size
 */
int simplify_iload_istore(CODE **c) {
  int a, b;
  if (is_iload(*c, &a) && is_istore(next(*c), &b) && a == b) {
    return replace(c, 2, makeCODEnop(NULL));
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
 * L1: (ref count --)
 *
 * L2: (ref count ++)
 *
 * Whatever things that were on the stack for the first comparison are consumed
 * exactly in the same way by the second comparison.
 *
 * Improvement:
 *      Reduces bytecode size
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



/*
 * goto L1 
 * ..
 * L1:
 * return
 * --------->
 * return
 * ..
 * L1:  (ref count --)
 *
 * Since the return will be called after the goto, we can call it right away.
 *
 * Improvement:
 *      Reduces bytecode size
 *
 */

int goto_return(CODE **c) {
  int l1;
  if(is_goto(*c, &l1) && is_return(next(destination(l1)))) {
    droplabel(l1);
    return replace(c, 1, makeCODEreturn(NULL));
  }
  return 0;
}

/*
 * goto L1 
 * [not label]
 * ...
 * --------->
 * goto L1 
 * ...
 *
 * Since there is a goto before the instruction, it will never be called.
 *
 * Improvement:
 *      Reduces bytecode size
 *
 */
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

/*
 * return/areturn/iremCK
 * [not label]
 * ...
 * --------->
 * return/areturn/iremCK
 * ...
 *
 * Since there is a return before the instruction, it will never be called.
 *
 * Improvement:
 *      Reduces bytecode size
 *
 */
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


/* iconst_0                     [ 0 ]
 * if_icmpeq/if_icmpne L1       [ * ]   and go to L1
 * ---------->
 * ifeq/ifne L1                 [ * ]   and go to L1
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

/* aconst_null                  [null]
 * if_acmpeq/if_acmpne L1       [ * ]   and go to L1
 * ---------->
 * ifnull/ifnonnull L1          [ * ]   and go to L1
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


/* Checks if some code is:
 * an expression: does not use the stack beneath it and pushes a single value
 * pure: no side effect
 * instruction: a single instruction
 * */
int is_pure_expression_instruction(CODE *c) {
  int d;
  char *d2;
  return (is_ldc_int(c, &d) || is_ldc_string(c,&d2) || is_aconst_null(c) ||
      is_aload(c, &d) || is_iload(c, &d));
}

/* pure_expression_instruction #1       [ a * ]
 * pure_expression_instruction #2       [ a b ]
 * swp                                  [ b a ]
 * ---------->
 * pure_expression_instruction #2       [ b * ]
 * pure_expression_instruction #1       [ a b ]
 *
 * We remove the swap by swapping the instructions. It is safe because the
 * instructions have no side-effects other than adding a single value onto the
 * stack
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

#define N_LOOKAHEAD 200

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
 * istore/astoke/iinc  k
 *
 * If the current operation is a store, it searches through a fixed set of
 * paths to see if it is ever loaded. If all the branches terminate or have the
 * variable stored again, then we know the current load will never be used. It
 * is then safe to remove the store.
 */
int remove_dead_store(CODE **c) {
  int k;
  int d; /* dummy */
  int instruction_count = N_LOOKAHEAD;
  if ((is_istore(*c, &k) || is_astore(*c, &k) || is_iinc(*c, &k, &d))
      && check_no_loads(next(*c), k, &instruction_count)) {
    return replace(c, 1, makeCODEpop(NULL));
  }
  return 0;
}

/* ldc "some litteral"  [ s * ]
 * dup                  [ s s ]
 * ifnonnull L1         [ s * ] and goes to L1
 * ---------->
 * ldc "some litteral"  [ s * ]
 * goto L1              [ s * ] and goes to L1
 *
 * We are allowed to ignore the null check because a string literal is not null.
 */

int simplify_ldc_string_ifnonnull(CODE **c) {
  char *s;
  int l;
  if (is_ldc_string(*c, &s) && is_dup(next(*c)) && is_ifnonnull(next(next(*c)), &l)){
    return replace(c, 3, makeCODEldc_string(s, makeCODEgoto(l, NULL)));
  }
  return 0;
}

/* [before]         [ s1 s2 ]
 * invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;      [ s3 * ]
 * dup              [ s3 s3 ]
 * ifnonnull L1     [ s3 *  ]   and go to label L1
 * ---------->
 * [before]         [ s1 s2 ]
 * invokevirtual java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;      [ s3 *] 
 * goto L1          [ s3 *  ]   and go to label L1   
 *
 * Here we assume that a string concatenation will never return a null.
 * Even if its arguments are null, it will not return a null but throw an exception instead.
 *
 *
 *
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
 *
 */
int remove_unnecessary_goto(CODE **c) {
  int l1, l2;
  if (is_goto(*c, &l1) && is_label(next(*c), &l2) && l1 == l2) {
    return replace_modified(c, 1, NULL);
  }
  return 0;
}


/* pure_expression_instruction      [ a ]
 * pop                              [ * ]
 * ---------->
 * nop                              [ * ]
 *
 *
 */
int basic_expression_pop(CODE **c) {
  if (is_pure_expression_instruction(*c) &&
      is_pop(next(*c))) {
    return replace(c, 2, makeCODEnop(NULL));
  }
  return 0;
}



/* Helper functions to check if two instructions are of a certain kind and are the same:
*/
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

/* Instructions that we beleive are safe to factor
 *  - Those that only consume integers
 *  - Those that the comsume values of types that are put back on the stack
 *  - Accessing locals
 *
 */
int instructions_equal_and_safe_to_factor(CODE *a, CODE *b) {
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
    check_and_compare(is_return, a, b) ||
    check_and_compare(is_dup, a, b) ||
    check_and_compare(is_swap, a, b) ||

    check_and_compare_int(is_ifeq, a, b) ||
    check_and_compare_int(is_ifne, a, b) ||

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

    check_and_compare_string(is_ldc_string, a, b) ||

    (is_iinc(a, &xa, &ya) && is_iinc(b, &xb, &yb) && xa==ya && xb==yb)
    ;
}

/* Instructions that we beleive are probably safe to factor are those that concern methods and fields.
 * Even though the same instruction may be exectuted on different types, we
 * believe those types are safe to merge from different branches.
 */
int instructions_equal_and_probably_safe_to_factor(CODE *a, CODE *b) {
  if (a->kind != b->kind) return 0;
  return 
    check_and_compare_string(is_getfield, a, b) ||
    check_and_compare_string(is_putfield, a, b) ||
    check_and_compare_string(is_invokevirtual, a, b)
    ;
}


/*
 * instruction A        
 * goto L1              
 * ...
 * instruction A        
 * goto L1 / L1:         (Either a goto to L1 or the label L1 itself)
 * ...
 * L1:                  
 * --------->
 *
 * goto L3              
 * ...
 * L3:(new label)       
 * instruction A
 * goto L1 / L1:
 * ...
 * L1 (reference count reduced by 1)
 *
 *
 * If we assume that the JVM does not verify code but otherwise executes every
 * instruction as it claims, then this would be sound since exactly the same
 * instructions would be executed in both cases.
 *
 * However, we must also pass JVM verification, which checks that the stack
 * values have the same types on branch merge. This is problematic because
 * there are JVM instructions that can operate on different types. For example,
 * if in one branch we pop an object of class A and in another branch we pop an object
 * of class B, if we factor out the pop instruction, we will not pass
 * verification because stack types will not match.
 *
 * We have two variants of this optimization: the non-risky and the risky one.
 * The only difference between them is the types of instructions they factor.
 *
 * Improvement:
 *      Reduces bytecode size
 */
int factor_instruction_generic(CODE **c, int equal_and_safe(CODE *a, CODE *b)) {
  CODE *p, *prev;
  int l1, l2, l3;
  CODE *l3_code;
  int d; /*dummy*/
  if ((!is_label(*c,&d)) &&
        is_goto(next(*c), &l1)){
    prev = next(*c);
    p = next(next(*c));
    while(p!=NULL) {
      if (equal_and_safe(*c, p) &&
          (is_goto(next(p), &l2) || is_label(next(p),&l2)) && l1==l2 ) {
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

int factor_instruction(CODE **c) {
    return factor_instruction_generic(c, instructions_equal_and_safe_to_factor);
}
int factor_instruction_risky(CODE **c) {
    return factor_instruction_generic(c, instructions_equal_and_probably_safe_to_factor);
}



/*
 * instruction A        
 * L1:
 * ...
 * instruction A        
 * goto L1
 * ----------->
 * L3:              (new label)
 * instruction A        
 * L1:              (ref count --)
 * ...
 * goto L3
 *
 * Mostly the sanme as the factor_instruction. similarly it has two variants.
 */
int factor_instruction_generic2(CODE **c, int equal_and_safe( CODE *, CODE *)) {
  CODE *p, *prev;
  int l1,l2, l3;
  CODE *l3_code;
  int d; /*dummy*/
  if ((!is_label(*c,&d)) &&
        is_label(next(*c), &l1)){

    prev = next(*c);

    p = next(next(*c));

    while(p!=NULL) {
      if (equal_and_safe(*c, p) &&
          is_goto(next(p), &l2) && l1==l2 ) {

          printf("ye\n");

        l3 = next_label();
        l3_code = makeCODElabel(l3, *c);

        *c = l3_code;

        INSERTnewlabel(l3,"factoring",l3_code,1);
        droplabel(l1);
        replace(&(prev->next), 2, makeCODEgoto(l3, NULL));
        return 1;
      }
      prev = p;
      p = next(p);
    }
  }
  return 0;
}

int factor_instruction2(CODE **c) {
    return factor_instruction_generic2(c, instructions_equal_and_safe_to_factor);
}
int factor_instruction2_risky(CODE **c) {
    return factor_instruction_generic2(c, instructions_equal_and_probably_safe_to_factor);
}

/* 
 * nop
 * [not end of method]
 * --------->
 * [nothing]
 *
 *
 *
 */
int remove_nop(CODE **c) {
    if (next(*c) != NULL && is_nop(*c)) {
        return replace(c, 1, NULL);
    }

    return 0;
}



void init_patterns(void) {
  ADD_PATTERN(constant_fold);
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
	ADD_PATTERN(factor_instruction);
	ADD_PATTERN(factor_instruction2);
	ADD_PATTERN(factor_instruction_risky);
	ADD_PATTERN(factor_instruction2_risky);
    ADD_PATTERN(remove_nop);
}
