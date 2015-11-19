#ifndef _ATOMIC_INCLUDED_
#define _ATOMIC_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
	uint8_t   InterlockedExchange8 (volatile uint8_t * target, uint8_t value) ;
	uint16_t  InterlockedExchange16 (volatile uint16_t * target, uint16_t value) ;
	uint32_t  InterlockedExchange32 (volatile uint32_t * target, uint32_t value) ;
#ifdef __x86_64__
	uint64_t  InterlockedExchange64 (volatile uint64_t * target, uint64_t value) ;
#endif
	
//    int32_t   HXAtomicIncRetINT32(int32_t* pNum);
//    int32_t   HXAtomicDecRetINT32(int32_t* pNum);
//
//    int32_t   InterlockedAdd(int32_t i, int32_t *v);
//    int32_t   InterlockedSub(int32_t i, int32_t *v);
//    int64_t   InterlockedAdd64(int64_t i, int64_t *v);
//    int64_t   InterlockedSub64(int64_t i, int64_t *v);
//    
//    int64_t InterlockedIncrement64( int64_t *v );
//    int64_t InterlockedDecrement64( int64_t *v );

#ifdef __cplusplus
}
#endif

#endif // _ATOMIC_INCLUDED_
