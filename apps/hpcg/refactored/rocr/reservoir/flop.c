// IRAD rights
#define INCLUDE_FLOP_C

#define add_flop_f  1
#define add_flop_d  1
#define sub_flop_f  1
#define sub_flop_d  1
#define mul_flop_f  1
#define mul_flop_d  1
#define div_flop_f  1
#define div_flop_d  1
#define sqrt_flop_f  10
#define sqrt_flop_d  16
#define cos_flop_f  10
#define cos_flop_d  16


// assigns the right values to
#ifdef INCLUDE_FLOP_C
#ifndef __MC_FLOP_DEF
#define __MC_FLOP_DEF
// flop counts per operation type
#ifndef MC_DOUBLE_PRECISION
int add_flop = add_flop_f;
int sub_flop = sub_flop_f;
int mul_flop = mul_flop_f;
int div_flop = div_flop_f;
int sqrt_flop = sqrt_flop_f;
int cos_flop = cos_flop_f;
#else
int add_flop = add_flop_d;
int sub_flop = sub_flop_d;
int mul_flop = mul_flop_d;
int div_flop = div_flop_d;
int sqrt_flop = sqrt_flop_d;
int cos_flop = cos_flop_d;
#endif // MC_DOUBLE_PRECISION
#endif // __MC_FLOP_DEF
#endif // INCLUDE_FLOP_C
