#ifndef _CPPSINK_INCLUDED_
#define _CPPSINK_INCLUDED_

#include <string.h>

typedef unsigned char BYTE;

#define CPP_SINK_BEGIN( SinkName ) \
	template< class CHost, class ISink > \
	class SinkName : public ISink \
	{ \
	public: \
		SinkName( CHost *pHost ) \
		{ \
		memset((BYTE*)&m_pHost, 0, sizeof(*this) - ((BYTE*)&m_pHost - (BYTE*)this)); \
		m_pHost = pHost; \
		} \
		virtual	~SinkName() \
		{ \
		} \
		void UnhookAll() \
		{ \
		CHost*	pHost = m_pHost; \
		memset((BYTE*)&m_pHost, 0, sizeof(*this) - ((BYTE*)&m_pHost - (BYTE*)this)); \
		m_pHost = pHost; \
		} \
	public: \
	CHost*	m_pHost; \

#define CPP_SINK_END() \
	}; \

/*	memset((BYTE*)&m_pHost, 0, sizeof(*this) - ((BYTE*)&m_pHost - (BYTE*)this)); 
的作用是将所有成员变量赋0, m_pHost 位于地址最小端,声明于其后的成员变量地址依次增大
*/

#define CPP_SINK_FUNC_VOID_RET(func, params, values) \
public: \
	typedef void (CHost::*F##func)params; \
	\
	void Hook_##func(F##func pf##func) \
	{ \
	m_pf##func		= pf##func; \
	} \
	\
	void Unhook_##func() \
	{ \
	m_pf##func	= NULL; \
	} \
	\
	virtual void (func)params \
	{ \
	if (m_pHost && m_pf##func) \
		{ \
		( (m_pHost)->*(m_pf##func))values; \
		} \
	} \
	\
private: \
	F##func	m_pf##func; \


#define CPP_SINK_FUNC( RetType, FuncName, ParamDeclare, CallArg ) \
public: \
	typedef RetType (CHost::*F##FuncName)ParamDeclare; \
	\
	void Hook_##FuncName(F##FuncName pf##FuncName) \
	{ \
	m_pf##FuncName		= pf##FuncName; \
	} \
	\
	void Unhook_##FuncName() \
	{ \
	m_pf##FuncName	= NULL; \
	} \
	\
	virtual RetType FuncName ParamDeclare \
	{ \
	if (m_pHost && m_pf##FuncName) \
		{ \
		return ( (m_pHost)->*(m_pf##FuncName))CallArg; \
		} \
		return (RetType)(-1); \
	} \
	\
private: \
	F##FuncName	m_pf##FuncName; \


#define DECLARE_CPP_SINK_OBJ( SinkName, ISink ) \
	template< class CHost > \
	class SinkName##Obj : public SinkName< CHost, ISink > \
	{ \
		public: \
			SinkName##Obj( CHost *pHost ) : SinkName< CHost, ISink >( pHost ) \
			{ \
			} \
			virtual ~SinkName##Obj() \
			{ \
			} \
	}; \


#endif
