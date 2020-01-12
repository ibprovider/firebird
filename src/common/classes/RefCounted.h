/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Vlad Horsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Vlad Horsun <hvlad@users.sf.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  Dmitry Yemanov <dimitr@users.sf.net>
 */

#ifndef COMMON_REF_COUNTED_H
#define COMMON_REF_COUNTED_H

#include "../common/classes/fb_atomic.h"
#include "../common/gdsassert.h"

namespace Firebird
{
	class RefCounted
	{
	public:
		virtual int addRef() const
		{
			fb_assert(m_debug__WAS_DELETED == 0);

			return ++m_refCnt;
		}

		virtual int release() const
		{
			fb_assert(m_refCnt.value() > 0);
			fb_assert(m_debug__WAS_DELETED == 0);

			const int refCnt = --m_refCnt;
			if (!refCnt)
			{
                fb_assert(::InterlockedIncrement(&m_debug__WAS_DELETED)==1);

            	delete this;
            }//if

			return refCnt;
		}

	protected:
		RefCounted()
		 : m_refCnt(0)
#ifdef DEV_BUILD
		 , m_debug__WAS_DELETED(0)
#endif
		{
		}

		virtual ~RefCounted()
		{
			fb_assert(m_refCnt.value() == 0);

#ifdef DEV_BUILD
            ::InterlockedIncrement(&m_debug__WAS_DELETED);
#endif
		}

	private:
		mutable AtomicCounter m_refCnt;

#ifdef DEV_BUILD
		mutable long m_debug__WAS_DELETED;
#endif
	};

	// reference counted object guard
	class Reference
	{
	public:
		explicit Reference(RefCounted& refCounted) :
			r(refCounted)
		{
			r.addRef();
		}

		~Reference()
		{
			try {
				r.release();
			}
			catch (const Exception&)
			{
				DtorException::devHalt();
			}
		}

	private:
		RefCounted& r;
	};

	enum NoIncrement {REF_NO_INCR};

	// controls reference counter of the object where points
	template <typename T>
	class RefPtr
	{
	public:
		RefPtr()
         : ptr(NULL)
#ifdef DEV_BUILD
         , m_debug__WAS_DELETED(0)
#endif
		{ }

		explicit RefPtr(T* p)
         : ptr(p)
#ifdef DEV_BUILD
         , m_debug__WAS_DELETED(0)
#endif
		{
			if (ptr)
			{
				ptr->addRef();
			}
		}

		// This special form of ctor is used to create refcounted ptr from interface,
		// returned by a function (which increments counter on return)
		RefPtr(NoIncrement x, T* p)
         : ptr(p)
#ifdef DEV_BUILD
         , m_debug__WAS_DELETED(0)
#endif
		{ }

		RefPtr(const RefPtr& r)
         : ptr(r.ptr)
#ifdef DEV_BUILD
         , m_debug__WAS_DELETED(0)
#endif
		{
			if (ptr)
			{
				ptr->addRef();
			}
		}

		~RefPtr()
		{
#ifdef DEV_BUILD
            const auto x=::InterlockedIncrement(&m_debug__WAS_DELETED);

            fb_assert(x==1);
            fb_assert(m_debug__WAS_DELETED==1);
#endif

			if (ptr)
			{
				ptr->release();
			}
		}

		T* assignRefNoIncr(T* p)
		{
			assign(NULL);
			ptr = p;
			return ptr;
		}

		T* operator=(T* p)
		{
			return assign(p);
		}

		T* operator=(const RefPtr& r)
		{
			return assign(r.ptr);
		}

		operator T*()
		{
			return ptr;
		}

		T* operator->()
		{
			return ptr;
		}

		operator const T*() const
		{
			return ptr;
		}

		const T* operator->() const
		{
			return ptr;
		}

		/* NS: you cannot have operator bool here. It creates ambiguity with
		  operator T* with some of the compilers (at least VS2003)

		operator bool() const
		{
			return ptr ? true : false;
		}*/

		bool hasData() const
		{
			return ptr ? true : false;
		}

		bool operator !() const
		{
			return !ptr;
		}

		bool operator ==(const RefPtr& r) const
		{
			return ptr == r.ptr;
		}

		bool operator !=(const RefPtr& r) const
		{
			return ptr != r.ptr;
		}

		T* getPtr()
		{
			return ptr;
		}

	private:
        template<class T>
        struct no_const
        {
         typedef T result;
        };

        template<class T>
        struct no_const<const T>
        {
         typedef T result;
        };


		T* assign(T* const p)
		{
			if (ptr == p)
			    return ptr;

            typedef  typename no_const<T>::result TT;

			if (p)
			{
				p->addRef();
			}

			T* tmp = (T*)::InterlockedExchangePointer(reinterpret_cast<void**>(const_cast<TT**>(&ptr)),
                                                      const_cast<TT*>(p));

			if (tmp)
			{
				tmp->release();
			}

			return p;
		}

		T* ptr;

#ifdef DEV_BUILD
		long m_debug__WAS_DELETED;
#endif
	};

	template <typename T>
	class AnyRef : public T, public RefCounted
	{
	public:
		inline AnyRef() : T() {}
		inline AnyRef(const T& v) : T(v) {}
		inline explicit AnyRef(MemoryPool& p) : T(p) {}
		inline AnyRef(MemoryPool& p, const T& v) : T(p, v) {}
	};
} // namespace

#endif // COMMON_REF_COUNTED_H
