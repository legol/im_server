#include <stdint.h>
#include "atomic.h"

#define xchg(ptr,v) ((__typeof__(*(ptr)))__xchg((unsigned long)(v),(ptr),sizeof(*(ptr))))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((struct __xchg_dummy *)(x))

/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway
 * Note 2: xchg has side effect, so that attribute volatile is necessary,
 *    but generally the primitive is invalid, *ptr is output argument. --ANK
 */
static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
    switch (size) {
        case 1:
            __asm__ __volatile__("xchgb %b0,%1"
                :"=q" (x)
                :"m" (*__xg(ptr)), "0" (x)
                :"memory");
            break;
        case 2:
            __asm__ __volatile__("xchgw %w0,%1"
                :"=r" (x)
                :"m" (*__xg(ptr)), "0" (x)
                :"memory");
            break;
        case 4:
            __asm__ __volatile__("xchgl %k0,%1"
                :"=r" (x)
                :"m" (*__xg(ptr)), "0" (x)
                :"memory");
            break;  
            
#ifdef __x86_64__
        case 8:
            __asm__ __volatile__("xchgq %0,%1"
                :"=r" (x)
                :"m" (*__xg(ptr)), "0" (x)
                :"memory");
            break;
#endif
    }

    return x;
}


uint8_t InterlockedExchange8 (volatile uint8_t * target, uint8_t value) 
{
    return xchg(target, value) ;
}

uint16_t InterlockedExchange16 (volatile uint16_t * target, uint16_t value) 
{
    return xchg(target, value) ;
}

uint32_t InterlockedExchange32 (volatile uint32_t * target, uint32_t value)
{
    return xchg(target, value) ;
}

#ifdef __x86_64__
uint64_t InterlockedExchange64 (volatile uint64_t * target, uint64_t value)
{
    return xchg(target, value) ;
}
#endif

	
///* Increment by 1 and return new value */
//int32_t HXAtomicIncRetINT32(int32_t* pNum)
//{
//    volatile int32_t ulRet;
//    __asm__ __volatile__(
//        "lock xaddl %%ebx, (%%rax);"     // atomically add 1 to *pNum
//        "     incl  %%ebx;"              // old value in %%ebx,increment it
//        : "=b" (ulRet)
//        : "a" (pNum), "b" (0x1)
//        : "cc", "memory"
//        );
//    return ulRet;
//}
//
///* Decrement by 1 and return new value */
//int32_t HXAtomicDecRetINT32(int32_t* pNum)
//{
//    volatile int32_t ulRet;
//    __asm__ __volatile__(
//        "lock xaddl %%ebx, (%%rax);"     // atomically add 1 to *pNum
//        "     decl  %%ebx;"              // old value in %%ebx,decrement it
//        : "=b" (ulRet)
//        : "a" (pNum), "b" (-1)
//        : "cc", "memory"
//        );
//    
//    return ulRet;
//}
//
//#define LOCK_PREFIX "lock ; "
//
//int32_t InterlockedAdd(int32_t i, int32_t *v)
//{
//    int32_t __i = i;
//    __asm__ __volatile__(
//        LOCK_PREFIX "xaddl %0, %1"
//        :"+r" (i), "+m" (*v)
//        : : "memory");
//    return i + __i;
//}
//
//int32_t InterlockedSub(int32_t i, int32_t *v)
//{
//    return InterlockedAdd( -1 * i,v);
//}
//
//int64_t InterlockedAdd64(int64_t i, int64_t *v)
//{
//    int64_t __i = i;
//    __asm__ __volatile__(
//        LOCK_PREFIX "xaddq %0, %1;"
//        :"=r"(i)
//        :"m"(*v), "0"(i));
//    return i + __i;
//}
//
//int64_t InterlockedSub64(int64_t i, int64_t *v)
//{
//    return InterlockedAdd64( -1 * i, v);
//}
//
//int64_t InterlockedIncrement64( int64_t *v )
//{
//    return InterlockedAdd64( 1ll, v );
//}
//
//int64_t InterlockedDecrement64( int64_t *v )
//{
//    return InterlockedSub64( 1ll, v );
//}
//
