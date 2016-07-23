#ifndef _OPCODE_H
#define _OPCODE_H


#include <stdint.h>
#include <avm.h>
#include "../ops.h"

#if !FREESTANDING
#include <string.h>
#include <math.h>
#endif

#if defined(__i386__)
#define __FASTCALL		__attribute__((fastcall))
#else
#define __FASTCALL
#endif

#define JPUSH(t, v)	\
	{ j->frame.stack[j->frame.regs.sp].i64 = 0LL; j->frame.stack[j->frame.regs.sp++].t = (v); }

#define JPUSH_JV(v)	\
	{ j->frame.stack[j->frame.regs.sp++].i64 = (v).i64; }


#define JPOP(t)		\
	j->frame.stack[--j->frame.regs.sp].t

#define JPOP_JV()	\
	j->frame.stack[--j->frame.regs.sp]



#define OPCODE(n)	\
	static __FASTCALL inline void j_op_##n (java_context_t* j)

#define OP(n)		\
	j_op_##n


#define PB			j->frame.regs.pb
#define PC			j->frame.regs.pc
#define SB			j->frame.regs.sb
#define SP			j->frame.regs.sp

#define PC8			j->frame.code[j->frame.regs.pc]
#define PC16		SWAP(*(u2*) (&j->frame.code[j->frame.regs.pc]), 16)
#define PC32		SWAP(*(u4*) (&j->frame.code[j->frame.regs.pc]), 32)
#define PC64		SWAP(*(u8*) (&j->frame.code[j->frame.regs.pc]), 64)



#define JGOTO(x)			\
	{ PC = PB + x; }

#define JRETURN				\
	{ j->flags = JAVACTX_FLAG_RETURN; }


#define ATHROW(x, y)			\
	{							\
		athrow(j, x, y);		\
		return;					\
	}


#ifdef _WITH_OPCODES
#include "nop.h"
#include "const.h"
#include "ldc.h"
#include "push.h"
#include "locals.h"
#include "array.h"
#include "stack.h"
#include "add.h"
#include "sub.h"
#include "mul.h"
#include "div.h"
#include "rem.h"
#include "neg.h"
#include "shift.h"
#include "and.h"
#include "or.h"
#include "xor.h"
#include "inc.h"
#include "conv.h"
#include "cmp.h"
#include "if.h"
#include "branch.h"
#include "fields.h"
#include "invoke.h"
#include "new.h"
#include "object.h"
#include "monitor.h"
#include "wide.h"
#include "breakpoint.h"
#endif

typedef __FASTCALL void (*opcode_handler_t) (java_context_t*);
typedef struct opcode {
	char* name;
	opcode_handler_t handler;
} opcode_t;


#ifdef _WITH_OPCODES
#define _OP(x)	\
	{ #x, OP(x) }
#else
#define _OP(x)	\
	{ #x, NULL }
#endif


static opcode_t j_opcodes[255] = {
	_OP(nop),
	_OP(aconst_null),
	_OP(iconst_m1),
	_OP(iconst_0),
	_OP(iconst_1),
	_OP(iconst_2),
	_OP(iconst_3),
	_OP(iconst_4),
	_OP(iconst_5),
	_OP(lconst_0),
	_OP(lconst_1),
	_OP(fconst_0),
	_OP(fconst_1),
	_OP(fconst_2),
	_OP(dconst_0),
	_OP(dconst_1),
	_OP(bipush),
	_OP(sipush),
	_OP(ldc),
	_OP(ldc_w),
	_OP(ldc2_w),
	_OP(iload),
	_OP(lload),
	_OP(fload),
	_OP(dload),
	_OP(aload),
	_OP(iload_0),
	_OP(iload_1),
	_OP(iload_2),
	_OP(iload_3),
	_OP(lload_0),
	_OP(lload_1),
	_OP(lload_2),
	_OP(lload_3),
	_OP(fload_0),
	_OP(fload_1),
	_OP(fload_2),
	_OP(fload_3),
	_OP(dload_0),
	_OP(dload_1),
	_OP(dload_2),
	_OP(dload_3),
	_OP(aload_0),
	_OP(aload_1),
	_OP(aload_2),
	_OP(aload_3),
	_OP(iaload),
	_OP(laload),
	_OP(faload),
	_OP(daload),
	_OP(aaload),
	_OP(baload),
	_OP(caload),
	_OP(saload),
	_OP(istore),
	_OP(lstore),
	_OP(fstore),
	_OP(dstore),
	_OP(astore),
	_OP(istore_0),
	_OP(istore_1),
	_OP(istore_2),
	_OP(istore_3),
	_OP(lstore_0),
	_OP(lstore_1),
	_OP(lstore_2),
	_OP(lstore_3),
	_OP(fstore_0),
	_OP(fstore_1),
	_OP(fstore_2),
	_OP(fstore_3),
	_OP(dstore_0),
	_OP(dstore_1),
	_OP(dstore_2),
	_OP(dstore_3),
	_OP(astore_0),
	_OP(astore_1),
	_OP(astore_2),
	_OP(astore_3),
	_OP(iastore),
	_OP(lastore),
	_OP(fastore),
	_OP(dastore),
	_OP(aastore),
	_OP(bastore),
	_OP(castore),
	_OP(sastore),
	_OP(pop),
	_OP(pop2),
	_OP(dup),
	_OP(dup_x1),
	_OP(dup_x2),
	_OP(dup2),
	_OP(dup2_x1),
	_OP(dup2_x2),
	_OP(swap),
	_OP(iadd),
	_OP(ladd),
	_OP(fadd),
	_OP(dadd),
	_OP(isub),
	_OP(lsub),
	_OP(fsub),
	_OP(dsub),
	_OP(imul),
	_OP(lmul),
	_OP(fmul),
	_OP(dmul),
	_OP(idiv),
	_OP(ldiv),
	_OP(fdiv),
	_OP(ddiv),
	_OP(irem),
	_OP(lrem),
	_OP(frem),
	_OP(drem),
	_OP(ineg),
	_OP(lneg),
	_OP(fneg),
	_OP(dneg),
	_OP(ishl),
	_OP(lshl),
	_OP(ishr),
	_OP(lshr),
	_OP(iushr),
	_OP(lushr),
	_OP(iand),
	_OP(land),
	_OP(ior),
	_OP(lor),
	_OP(ixor),
	_OP(lxor),
	_OP(iinc),
	_OP(i2l),
	_OP(i2f),
	_OP(i2d),
	_OP(l2i),
	_OP(l2f),
	_OP(l2d),
	_OP(f2i),
	_OP(f2l),
	_OP(f2d),
	_OP(d2i),
	_OP(d2l),
	_OP(d2f),
	_OP(i2b),
	_OP(i2c),
	_OP(i2s),
	_OP(lcmp),
	_OP(fcmpl),
	_OP(fcmpg),
	_OP(dcmpl),
	_OP(dcmpg),
	_OP(ifeq),
	_OP(ifne),
	_OP(iflt),
	_OP(ifge),
	_OP(ifgt),
	_OP(ifle),
	_OP(if_icmpeq),
	_OP(if_icmpne),
	_OP(if_icmplt),
	_OP(if_icmpge),
	_OP(if_icmpgt),
	_OP(if_icmple),
	_OP(if_acmpeq),
	_OP(if_acmpne),
	_OP(goto),
	_OP(jsr),
	_OP(ret),
	_OP(tableswitch),
	_OP(lookupswitch),
	_OP(ireturn),
	_OP(lreturn),
	_OP(freturn),
	_OP(dreturn),
	_OP(areturn),
	_OP(return),
	_OP(getstatic),
	_OP(putstatic),
	_OP(getfield),
	_OP(putfield),
	_OP(invokevirtual),
	_OP(invokespecial),
	_OP(invokestatic),
	_OP(invokeinterface),
	_OP(invokedynamic),
	_OP(new),
	_OP(newarray),
	_OP(anewarray),
	_OP(arraylength),
	_OP(athrow),
	_OP(checkcast),
	_OP(instanceof),
	_OP(monitorenter),
	_OP(monitorexit),
	_OP(wide),
	_OP(multinewarray),
	_OP(ifnull),
	_OP(ifnonnull),
	_OP(goto_w),
	_OP(jsr_w),
	_OP(breakpoint),
};

#endif
