#ifndef _SHARED_ARRAY_INCLUDED_
#define _SHARED_ARRAY_INCLUDED_

#include "shared_count.h"

#include <cstddef>            // for std::ptrdiff_t
#include <algorithm>          // for std::swap
#include <functional>         // for std::less

template<class T> struct checked_array_deleter
{
	typedef void result_type;
	typedef T * argument_type;

	void operator()(T * x) const
	{
		delete []x;
	}
};


template<class T> class shared_array
{
	private:
		typedef checked_array_deleter<T> deleter;
		typedef shared_array<T> this_type;

	public:

		typedef T element_type;

		explicit shared_array(T * p = 0): px(p), pn(p, deleter())
	{
	}

		//
		// Requirements: D's copy constructor must not throw
		//
		// shared_array will release p by calling d(p)
		//

		template<class D> shared_array(T * p, D d): px(p), pn(p, d)
	{
	}

		shared_array & operator=(shared_array const & r) // never throws
		{
			px = r.px;
			pn = r.pn; // shared_count::op= doesn't throw
			return *this;
		}

		template<class Y>
			shared_array & operator=(shared_array<Y> const & r) // never throws
			{
				px = r.px;
				pn = r.pn; // shared_count::op= doesn't throw
				return *this;
			}


		void reset(T * p = 0)
		{
			this_type(p).swap(*this);
		}

		template <class D> void reset(T * p, D d)
		{
			this_type(p, d).swap(*this);
		}


		T & operator[] (std::ptrdiff_t i) const // never throws
		{
			return px[i];
		}

		T * get() const // never throws
		{
			return px;
		}

		// implicit conversion to "bool"

		operator bool () const
		{
			return px != 0;
		}

		bool operator! () const // never throws
		{
			return px == 0;
		}

		bool unique() const // never throws
		{
			return pn.unique();
		}

		long use_count() const // never throws
		{
			return pn.use_count();
		}

		void swap(shared_array<T> & other) // never throws
		{
			std::swap(px, other.px);
			pn.swap(other.pn);
		}

	private:

		T * px;                     // contained pointer
		shared_count pn;    // reference counter

};  // shared_array

template<class T> inline bool operator==(shared_array<T> const & a, shared_array<T> const & b) // never throws
{
	return a.get() == b.get();
}

template<class T> inline bool operator!=(shared_array<T> const & a, shared_array<T> const & b) // never throws
{
	return a.get() != b.get();
}

template<class T> inline bool operator<(shared_array<T> const & a, shared_array<T> const & b) // never throws
{
	return std::less<T*>()(a.get(), b.get());
}

template<class T> void swap(shared_array<T> & a, shared_array<T> & b) // never throws
{
	a.swap(b);
}

#endif  // #ifndef _SHARED_ARRAY_INCLUDED_
