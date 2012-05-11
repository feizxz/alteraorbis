/*
Copyright (c) 2000-2009 Lee Thomason (www.grinninglizard.com)
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


#ifndef GRINNINGLIZARD_MICROPATHER_INCLUDED
#define GRINNINGLIZARD_MICROPATHER_INCLUDED


/** @mainpage MicroPather
	
	MicroPather is a path finder and A* solver (astar or a-star) written in platform independent 
	C++ that can be easily integrated into existing code. MicroPather focuses on being a path 
	finding engine for video games but is a generic A* solver. MicroPather is open source, with 
	a license suitable for open source or commercial use.

	An overview of using MicroPather is in the <A HREF="../readme.htm">readme</A> or
	on the Grinning Lizard website: http://grinninglizard.com/micropather/
*/

#ifdef GRINLIZ_NO_STL
#	define MP_VECTOR micropather::MPVector
#else
#	include <vector>
#	define MP_VECTOR std::vector
#endif
#include <float.h>

#ifdef _DEBUG
	#ifndef DEBUG
		#define DEBUG
	#endif
#endif


#if defined( _DEBUG )
#	if defined( _MSC_VER )
#		define MPASSERT( x )		if ( !(x)) { _asm { int 3 } }
#	else
#		include <assert.h>
#		define MPASSERT assert
#	endif
#else
#	define MPASSERT( x ) {}
#endif


#if defined(_MSC_VER) && (_MSC_VER >= 1400 )
	#include <stdlib.h>
	typedef uintptr_t		MP_UPTR;
#elif defined (__GNUC__) && (__GNUC__ >= 3 )
	#include <stdlib.h>
	typedef uintptr_t		MP_UPTR;
#else
	// Assume not 64 bit pointers. Get a new compiler.
	typedef unsigned MP_UPTR;
#endif

//#define MICROPATHER_STRESS

namespace micropather
{
#ifdef GRINLIZ_NO_STL

	/* WARNING: incompatible vector replacement. Does everything needed to replace std::vector
	   for micropather, but not a compatible replacement. MPVector doesn't correctly call constructors or destructors.
	 */
	template <typename T>
	class MPVector {
	public:
		MPVector() : m_allocated( 0 ), m_size( 0 ), m_buf ( 0 ) {}
		~MPVector()	{ delete [] m_buf; }

		void clear()						{ m_size = 0; }	// see warning above
		void resize( unsigned s )			{ capacity( s );
											  m_size = s;
											}	
		T& operator[](unsigned i)			{ MPASSERT( i>=0 && i<m_size );
											  return m_buf[i];
											}
		const T& operator[](unsigned i) const	{ MPASSERT( i>=0 && i<m_size );
												  return m_buf[i];
												}
		void push_back( const T& t )		{ capacity( m_size+1 );
											  m_buf[m_size++] = t;
											}
		unsigned size()	const				{ return m_size; }

	private:
		void capacity( unsigned cap ) {
			if ( m_allocated < cap ) { 
				unsigned newAllocated = cap * 3/2 + 16;
				T* newBuf = new T[newAllocated];
				MPASSERT( m_size <= m_allocated );
				MPASSERT( m_size < newAllocated );
				memcpy( newBuf, m_buf, sizeof(T)*m_size );
				delete [] m_buf;
				m_buf = newBuf;
				m_allocated = newAllocated;
			}
		}
		unsigned m_allocated;
		unsigned m_size;
		T* m_buf;
	};
#endif

	class PathNode;

	/**
		Used to pass the cost of states from the cliet application to MicroPather. This
		structure is copied in a vector.

		@sa AdjacentCost
	*/
	struct StateCost
	{
		PathNode* state;		///< The state as a void*
		float cost;				///< The cost to the state. Use FLT_MAX for infinite cost.
	};


	/**
		A pure abstract class used to define a set of callbacks. 
		The client application inherits from 
		this class, and the methods will be called when MicroPather::Solve() is invoked.

		The notion of a "state" is very important. It must have the following properties:
		- Unique
		- Unchanging (unless MicroPather::Reset() is called)

		If the client application represents states as objects, then the state is usually
		just the object cast to a void*. If the client application sees states as numerical
		values, (x,y) for example, then state is an encoding of these values. MicroPather
		never interprets or modifies the value of state.
	*/
	class Graph
	{
	  public:
		virtual ~Graph() {}
	  
		/**
			Return the least possible cost between 2 states. For example, if your pathfinding 
			is based on distance, this is simply the straight distance between 2 points on the 
			map. If you pathfinding is based on minimum time, it is the minimal travel time 
			between 2 points given the best possible terrain.
		*/
		virtual float LeastCostEstimate( PathNode* stateStart, PathNode* stateEnd ) = 0;

		/** 
			Return the exact cost from the given state to all its neighboring states. This
			may be called multiple times, or cached by the solver. It *must* return the same
			exact values for every call to MicroPather::Solve(). It should generally be a simple,
			fast function with no callbacks into the pather.
		*/	
		virtual void AdjacentCost( PathNode* state, micropather::StateCost **adjacent, int* nAdjacent ) = 0;

		/**
			This function is only used in DEBUG mode - it dumps output to stdout. Since void* 
			aren't really human readable, normally you print out some concise info (like "(1,2)") 
			without an ending newline.
		*/
		virtual void  PrintStateInfo( PathNode* state ) = 0;
	};


	class PathNode;

	struct NodeCost
	{
		PathNode* node;
		float cost;
	};


	/*
		Every state (void*) is represented by a PathNode in MicroPather. There
		can only be one PathNode for a given state.
	*/
	class PathNode
	{
	public:
	    virtual ~PathNode() {}

		void Init(	unsigned _frame,
					float _costFromStart, 
					float _estToGoal, 
					PathNode* _parent );

		void InitSentinel() {
			Init( 0, FLT_MAX, FLT_MAX, 0 );
			prev = next = this;
			frame = 0;
		}	

		unsigned frame;			// used to determine whether init should do anything
		float costFromStart;	// exact
		float estToGoal;		// estimated
		float totalCost;		// could be a function, but save some math.
		PathNode* parent;		// the parent is used to reconstruct the path

		PathNode *next, *prev;	// used by open queue

		bool inOpen;
		bool inClosed;

		void Unlink() {
			next->prev = prev;
			prev->next = next;
			next = prev = 0;
		}
		void AddBefore( PathNode* addThis ) {
			addThis->next = this;
			addThis->prev = prev;
			prev->next = addThis;
			prev = addThis;
		}
		#ifdef DEBUG
		void CheckList()
		{
			MPASSERT( totalCost == FLT_MAX );
			for( PathNode* it = next; it != this; it=it->next ) {
				MPASSERT( it->prev == this || it->totalCost >= it->prev->totalCost );
				MPASSERT( it->totalCost <= it->next->totalCost );
			}
		}
		#endif

		void CalcTotalCost() {
			if ( costFromStart < FLT_MAX && estToGoal < FLT_MAX )
				totalCost = costFromStart + estToGoal;
			else
				totalCost = FLT_MAX;
		}

	  private:

		void operator=( const PathNode& );
	};


	/**
		Create a MicroPather object to solve for a best path. Detailed usage notes are
		on the main page.
	*/
	class MicroPather
	{
		friend class micropather::PathNode;

	  public:
		enum
		{
			SOLVED,
			NO_SOLUTION,
			START_END_SAME,
		};

		/**
			Construct the pather, passing a pointer to the object that implements
			the Graph callbacks.

			@param graph		The "map" that implements the Graph callbacks.
			@param allocate		How many states should be internally allocated at a time. This
								can be hard to get correct. The higher the value, the more memory
								MicroPather will use.
								- If you have a small map (a few thousand states?) it may make sense
								  to pass in the maximum value. This will cache everything, and MicroPather
								  will only need one main memory allocation. For a chess board, allocate 
								  would be set to 8x8 (64)
								- If your map is large, something like 1/4 the number of possible
								  states is good. For example, Lilith3D normally has about 16,000 
								  states, so 'allocate' should be about 4000.
							    - If your state space is huge, use a multiple (5-10x) of the normal
								  path. "Occasionally" call Reset() to free unused memory.
			@param typicalAdjacent	Used to determine cache size. The typical number of adjacent states
									to a given state. (On a chessboard, 8.) Higher values use a little
									more memory.
		*/
		MicroPather( Graph* graph, unsigned allocate = 250, unsigned typicalAdjacent=6 );
		~MicroPather();

		/**
			Solve for the path from start to end.

			@param startState	Input, the starting state for the path.
			@param endState		Input, the ending state for the path.
			@param path			Output, a vector of states that define the path. Empty if not found.
			@param totalCost	Output, the cost of the path, if found.
			@return				Success or failure, expressed as SOLVED, NO_SOLUTION, or START_END_SAME.
		*/
		int Solve( PathNode* startState, PathNode* endState, MP_VECTOR< PathNode* >* path, float* totalCost );

		/**
			Find all the states within a given cost from startState.

			@param startState	Input, the starting state for the path.
			@param near			All the states within 'maxCost' of 'startState', and cost to that state.
			@param maxCost		Input, the maximum cost that will be returned. (Higher values return
								larger 'near' sets and take more time to compute.)
			@return				Success or failure, expressed as SOLVED or NO_SOLUTION.
		*/
		int SolveForNearStates( PathNode* startState, MP_VECTOR< StateCost >* near, float maxCost );

		/** Should be called whenever the cost between states or the connection between states changes.
			Also frees overhead memory used by MicroPather, and calling will free excess memory.
		*/
		void Reset();

		// Debugging function to return all states that were used by the last "solve" 
		//void StatesInPool( MP_VECTOR< void* >* stateVec );

	  private:
		MicroPather( const MicroPather& );	// undefined and unsupported
		void operator=( const MicroPather ); // undefined and unsupported
		
		void GoalReached( PathNode* node, PathNode* start, PathNode* end, MP_VECTOR< PathNode* > *path );

		MP_VECTOR< StateCost >	stateCostVec;	// local to Solve, but put here to reduce memory allocation

		unsigned frame;
		Graph* graph;
	};
};	// namespace grinliz

#endif

