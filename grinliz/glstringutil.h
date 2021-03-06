/*
Copyright (c) 2000 Lee Thomason (www.grinninglizard.com)
Grinning Lizard Utilities.

This software is provided 'as-is', without any express or implied 
warranty. In no event will the authors be held liable for any 
damages arising from the use of this software.

Permission is granted to anyone to use this software for any 
purpose, including commercial applications, and to alter it and 
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must 
not claim that you wrote the original software. If you use this 
software in a product, an acknowledgment in the product documentation 
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and 
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source 
distribution.
*/



#ifndef GRINLIZ_STRINGUTIL_INCLUDED
#define GRINLIZ_STRINGUTIL_INCLUDED

#ifdef _MSC_VER
#pragma warning( disable : 4530 )
#pragma warning( disable : 4786 )
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gldebug.h"
#include "gltypes.h"
#include "glcontainer.h"


namespace grinliz 
{

inline bool StrEqual( const char* a, const char* b ) 
{
	if ( a && b ) {
		if ( a == b ) 
			return true;
		else if ( strcmp( a, b ) == 0 )
			return true;
	}
	return false;
}


inline bool StrEqualUntil( const char* a, const char* b, char until ) 
{
	if ( a && b ) {
		if ( a == b ) 
			return true;
		const char* p = a;
		const char* q = b;
		while( *p && *q && (*p == *q) ) {
			++p;
			++q;
		}
		if (    (*p == 0 && *q == 0)				// they both hit null
             || (*p == until || *q== until ) )		// one or other reached terminator
			return true;
	}
	return false;
}


// Reimplements SAFE strncpy, safely cross-compiler. Always returns a null-terminated string.
void StrNCpy( char* dst, const char* src, size_t bufferSize );
int SNPrintf(char *str, size_t size, const char *format, ...);


/*
	A class that wraps a c-array of characters.
*/
template< int ALLOCATE >
class CStr
{
public:
	CStr()							{	GLASSERT(sizeof(*this) == ALLOCATE );		// not required for class to work, but certainly the intended design
										buf[0] = 0; 
										Validate();
									}
	CStr( const char* src )			{	GLASSERT(sizeof(*this) == ALLOCATE );
										*buf = 0;
										if ( src ) {
											GLASSERT( strlen( src ) < (ALLOCATE-1));
											StrNCpy( buf, src, ALLOCATE ); 
										}
										Validate();
									}
	CStr( const CStr<ALLOCATE>& other ) {
										memcpy( buf, other.buf, ALLOCATE );
										Validate();
									}

	~CStr()	{}

	const char* c_str()	const			{ return buf; }
	const char* safe_str() const		{ return buf ? buf : ""; }

	int size() const					{ return strlen( buf ); }
	bool empty() const					{ return buf[0] == 0; }

	int Length() const 					{ return strlen( buf ); }
	int Capacity() const				{ return ALLOCATE-1; }
	void ClearBuf()						{ memset( buf, 0, ALLOCATE ); }
	void Clear()						{ buf[0] = 0; }

	void Format( const char* format, ...) 
	{
		va_list     va;
		va_start( va, format );
	#ifdef _MSC_VER
		vsnprintf_s( buf, ALLOCATE, _TRUNCATE, format, va );
	#else
		vsnprintf( buf, ALLOCATE, format, va );
	#endif
		va_end( va );
		Validate();
	}

	void AppendFormat( const char* format, ... )
	{
		va_list     va;
		va_start( va, format );
		int s = size();
		if ( s < Capacity() ) {
			#ifdef _MSC_VER
				vsnprintf_s( buf+s, ALLOCATE-s, _TRUNCATE, format, va );
			#else
				vsnprintf( buf+s, ALLOCATE-s, format, va );
			#endif
		}
		va_end( va );
		Validate();
	}

	bool operator==( const char* str ) const						{ return buf && str && strcmp( buf, str ) == 0; }
	bool operator!=( const char* str ) const						{ return !(*this == str); }
	char operator[]( int i ) const									{ GLASSERT( i>=0 && i<ALLOCATE-1 ); return buf[i]; }
	char& operator[]( int i ) 										{ GLASSERT( i>=0 && i<ALLOCATE-1 ); return buf[i]; }
	template < class T > bool operator==( const T& str ) const		{ return buf && strcmp( buf, str.c_str() ) == 0; }

	void operator=( const char* src )	{	
		if (src && *src) {
			GLASSERT( strlen( src ) < (ALLOCATE-1) );
			StrNCpy( buf, src, ALLOCATE ); 
		}
		else {
			buf[0] = 0;
		}
		Validate();
	}
	
	void operator=( int value ) {
		SNPrintf( buf, ALLOCATE, "%d", value );
		Validate();
	}

	void operator+=( const char* src ) {
		GLASSERT( src );
		if ( src ) {
			int len = size();
			if ( len < ALLOCATE-1 )
				StrNCpy( buf+len, src, ALLOCATE-len );
		}
		Validate();
	}

	void operator+=( int c ) {
		GLASSERT( c > 0 );
		int len = size();
		if ( len < ALLOCATE-1 ) {
			buf[len] = (char)c;
			buf[len+1] = 0;
		}
		Validate();
	}

private:
#ifdef DEBUG
	void Validate() {
		GLASSERT( strlen(buf) < ALLOCATE );	// strictly less - need space for null terminator
	}
#else
	void Validate() {}
#endif
	char buf[ALLOCATE];
};


/*
	Simple replace for std::string, to remove stdlib dependencies.
*/
class GLString
{
public:
	GLString() : m_buf( (char*)nullBuf ), m_allocated( 0 ), m_size( 0 )							{}
	GLString( const GLString& rhs ) : m_buf( (char*)nullBuf ), m_allocated( 0 ), m_size( 0 )		{ init( rhs ); }
	GLString( const char* rhs ) : m_buf( (char*)nullBuf ), m_allocated( 0 ), m_size( 0 )			{ init( rhs ); }
	~GLString()																				{ if (m_buf != nullBuf ) delete [] m_buf; }

	void operator=( const GLString& rhs )		{ init( rhs ); }
	void operator=( const char* rhs )			{ init( rhs ); }

	void operator+= ( const GLString& rhs )		{ append( rhs ); }
	void operator+= ( const char* rhs )			{ append( rhs ); }
	void operator+= ( char c )					{ char s[2] = { c, 0 }; append( s ); }

	bool operator==( const GLString& rhs ) const	{ return compare( rhs.c_str() ) == 0; }
	bool operator==( const char* rhs ) const		{ return compare( rhs ) == 0; }
	bool operator!=( const GLString& rhs ) const	{ return compare( rhs.c_str() ) != 0; }
	bool operator!=( const char* rhs ) const		{ return compare( rhs ) != 0; }

	char operator[]( unsigned i ) const				{ GLASSERT( i < size() );
													  return m_buf[i];
													}
	char& operator[]( unsigned i )					{ GLASSERT( i < size() );
													  return m_buf[i];
													}

	unsigned find( char c )	const					{	const char* p = strchr( m_buf, c );
														return ( p ) ? (p-m_buf) : size();
													}
	unsigned rfind( char c )	const				{	const char* p = strrchr( m_buf, c );
														return ( p ) ? (p-m_buf) : size();
													}
	GLString substr( unsigned pos, unsigned n ) const;

	void append( const GLString& str );
	void append( const char* );
	void append( const char* p, int n );
	int compare( const char* str ) const			{ return strcmp( m_buf, str ); }
	bool empty() const								{ return m_size == 0; }
	void clear()									{ init(""); }

	unsigned size() const							{ return m_size; }
	const char* c_str() const						{ return m_buf; }
	const char* safe_str() const					{ return m_buf ? m_buf : ""; }

	void Format( const char* format, ...);
	void AppendFormat( const char* format, ...);

private:
	void ensureSize( unsigned s );
	void init( const GLString& str );
	void init( const char* );
#ifdef DEBUG	
	void validate() const;
#else
	void validate() const	{}
#endif

	static const char* nullBuf;
	char*		m_buf;
	unsigned	m_allocated;
	unsigned	m_size;
};


// Immutable, interned string
class IString
{
	friend class StringPool;
public:
	IString()									{ str = 0; }
	IString( const IString& other )				{ str = other.str; }	
	void operator=( const IString& other )		{ str = other.str; }

	bool operator==( const IString& other )	const	{ return other.str == str; }
	bool operator==( const GLString& other ) const  { return    (str == 0 && other.empty() )
															 || StrEqual( other.c_str(), str ); }
	bool operator==( const char* other ) const		{ return    (str == 0 && other == 0)
														     || StrEqual( other, str ); }

	bool operator!=( const IString& other ) const	{ return other.str != str; }
	bool operator!=( const GLString& other )  const	{ return !(*this == other); }
	bool operator!=( const char* other )  const		{ return !(*this == other); }

	bool operator<( const IString& other ) const	{ return strcmp( safe_str(), other.safe_str() ) < 0; } 
	bool operator>( const IString& other ) const	{ return strcmp( safe_str(), other.safe_str() ) > 0; } 

	const char* c_str() const					{ return str; }
	const char* safe_str() const				{ return str ? str : ""; }
	bool empty() const							{ return !str || *str == 0; }
	int size() const							{ return strlen( str ); }

private:
	IString( const char* _str )					{ str = _str; }
	const char* str;
};


class CompValueString {
public:
	// Hash table:
	template <class T>
	static U32 Hash(T& _p) {
		const char* p = _p.safe_str();
		U32 hash = 2166136261UL;
		for (; *p; ++p) {
			hash ^= *p;
			hash *= 16777619;
		}
		return hash;
	}
	template <class T>
	static bool Equal(const T& v0, const T& v1)	{ return v0 == v1; }
	template <class T>
	static bool Less(const T& v0, const T& v1)	{ return v0 < v1; }
};


class StringPool
{
public:
	static StringPool* Instance();

	// Create WITHOUT using instance.
	StringPool();
	~StringPool();

	IString Get( const char* str, bool strIsStaticMem=false );

	static IString Intern( const char* str, bool strIsStaticMem=false ) {
		return Instance()->Get( str, strIsStaticMem );
	}

	void GetAllStrings( CDynArray< const char* >* arr );
	void GetAllStrings( CDynArray< IString >* arr );

private:
	const char* Add( const char* str );

	static StringPool* instance;

	enum { BLOCK_SIZE = 4000 };
	
	struct Node {
		U32 hash;
		const char* str;
	};

	class CompValueNode {
	public:
		static U32 Hash(const Node& v) { return v.hash; }
		static bool Equal(const Node& v0, const Node& v1)	{ return v0.hash == v1.hash; }
		static bool Less(const Node& v0, const Node& v1)	{ return v0.hash < v1.hash; }
	};

	struct Block {
		Block* next;
		int nBytes;
		char mem[BLOCK_SIZE];
	};
	/*
		Originally implemented as a binary tree;
		but actual code hit the actual pathelogical
		case where the tree was one sided, and the 
		memory (allocated for the full tree) 
		consumed all available memory.

		Switched to sorted array.

		May wish to consider aa-tree.
	*/
	SortedDynArray<Node, ValueSem, CompValueNode> nodes;
	Block* root;

	int nBlocks;
};


void StrSplitFilename(	const GLString& fullPath, 
						GLString* basePath,
						GLString* name,
						GLString* extension );

void StrFillBuffer( const GLString& str, char* buffer, int bufferSize );
void StrFillBuffer( const char* str, char* buffer, int bufferSize );


struct StrToken {
	enum {
		UNKNOWN,
		STRING,
		NUMBER
	};

	int type;
	GLString str;
	double number;

	StrToken() : type( 0 ), number( 0.0 ) {}

	void InitString( const GLString& str ) {
		type = STRING;
		this->str = str;
	}
	void InitNumber( double num ) {
		type = NUMBER;
		this->number = num;
	}
};

void StrTokenize( const GLString& in, CDynArray<StrToken>* tokens, bool append=false );

};	// namespace grinliz


#endif
