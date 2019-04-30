/*
Copyright (c) 2000-2013 Lee Thomason (www.grinninglizard.com)
Micropather

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

  base commit: 33a3b8403f1bc3937c9d364fe6c3977169bee3b5
*/

// This probably works to remove, but isn't currently tested in STL mode.
#define GRINLIZ_NO_STL

#ifdef GRINLIZ_NO_STL
# define MP_VECTOR micropather::MPVector
#else
# error "GTFO, STL!"
#endif
#include <float.h>

#ifdef _DEBUG
  #ifndef DEBUG
    #define DEBUG
  #endif
#endif


#if defined(DEBUG)
#   if defined(_MSC_VER)
#       // "(void)0," is for suppressing C4127 warning in "assert(false)", "assert(true)" and the like
#       define MPASSERT( x )           if ( !((void)0,(x))) { __debugbreak(); } //if ( !(x)) WinDebugBreak()
#   elif defined (ANDROID_NDK)
#       include <android/log.h>
#       define MPASSERT( x )           if ( !(x)) { __android_log_assert( "assert", "grinliz", "ASSERT in '%s' at %d.", __FILE__, __LINE__ ); }
#   else
#       include <assert.h>
#       define MPASSERT                assert
#   endif
#   else
#       define MPASSERT( x )           {}
#endif


#if defined(_MSC_VER) && (_MSC_VER >= 1400 )
  #include <stdlib.h>
  typedef uintptr_t   MP_UPTR;
#elif defined (__GNUC__) && (__GNUC__ >= 3 )
  #include <stdint.h>
  #include <stdlib.h>
  typedef uintptr_t   MP_UPTR;
#else
  // Assume not 64 bit pointers. Get a new compiler.
  typedef unsigned MP_UPTR;
#endif

namespace micropather
{
#ifdef GRINLIZ_NO_STL

  /* WARNING: vector partial replacement. Does everything needed to replace std::vector
     for micropather, but only works on Plain Old Data types. Doesn't call copy/construct/destruct
     correctly for general use.
   */
  template <typename T>
  class MPVector {
  public:
    MPVector() : m_allocated( 0 ), m_size( 0 ), m_buf ( 0 ) {}
    ~MPVector() { delete [] m_buf; }

    void clear()            { m_size = 0; } // see warning above
    void resize( unsigned s )     { capacity( s );
                        m_size = s;
                      }
    T& operator[](unsigned i)     { MPASSERT( i>=0 && i<m_size );
                        return m_buf[i];
                      }
    const T& operator[](unsigned i) const { MPASSERT( i>=0 && i<m_size );
                          return m_buf[i];
                        }
    void push_back( const T& t )    { capacity( m_size+1 );
                        m_buf[m_size++] = t;
                      }
    unsigned size() const       { return m_size; }

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

  /**
    Used to pass the cost of states from the cliet application to MicroPather. This
    structure is copied in a vector.

    @sa AdjacentCost
  */
  struct StateCost
  {
    void* state;      ///< The state as a void*
    float cost;       ///< The cost to the state. Use FLT_MAX for infinite cost.
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
    virtual float LeastCostEstimate( void* stateStart, void* stateEnd ) = 0;

    /**
      Return the exact cost from the given state to all its neighboring states. This
      may be called multiple times, or cached by the solver. It *must* return the same
      exact values for every call to MicroPather::Solve(). It should generally be a simple,
      fast function with no callbacks into the pather.
    */
    virtual void AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent ) = 0;

    /**
      This function is only used in DEBUG mode - it dumps output to stdout. Since void*
      aren't really human readable, normally you print out some concise info (like "(1,2)")
      without an ending newline.
    */
    virtual void  PrintStateInfo( void* state ) = 0;
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
    void Init(  unsigned _frame,
          void* _state,
          float _costFromStart,
          float _estToGoal,
          PathNode* _parent );

    void Clear();
    void InitSentinel() {
      Clear();
      Init( 0, 0, FLT_MAX, FLT_MAX, 0 );
      prev = next = this;
    }

    void *state;      // the client state
    float costFromStart;  // exact
    float estToGoal;    // estimated
    float totalCost;    // could be a function, but save some math.
    PathNode* parent;   // the parent is used to reconstruct the path
    unsigned frame;     // unique id for this path, so the solver can distinguish
                // correct from stale values

    int numAdjacent;    // -1  is unknown & needs to be queried
    int cacheIndex;     // position in cache

    PathNode *child[2];   // Binary search in the hash table. [left, right]
    PathNode *next, *prev;  // used by open queue

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


  /* Memory manager for the PathNodes. */
  class PathNodePool
  {
  public:
    PathNodePool( unsigned allocate, unsigned typicalAdjacent );
    ~PathNodePool();

    // Free all the memory except the first block. Resets all memory.
    void Clear();

    // Essentially:
    // pNode = Find();
    // if ( !pNode )
    //    pNode = New();
    //
    // Get the PathNode associated with this state. If the PathNode already
    // exists (allocated and is on the current frame), it will be returned.
    // Else a new PathNode is allocated and returned. The returned object
    // is always fully initialized.
    //
    // NOTE: if the pathNode exists (and is current) all the initialization
    //       parameters are ignored.
    PathNode* GetPathNode(    unsigned frame,
                  void* _state,
                  float _costFromStart,
                  float _estToGoal,
                  PathNode* _parent );

    // Get a pathnode that is already in the pool.
    PathNode* FetchPathNode( void* state );

    // Store stuff in cache
    bool PushCache( const NodeCost* nodes, int nNodes, int* start );

    // Get neighbors from the cache
    // Note - always access this with an offset. Can get re-allocated.
    void GetCache( int start, int nNodes, NodeCost* nodes );

    // Return all the allocated states. Useful for visuallizing what
    // the pather is doing.
    void AllStates( unsigned frame, MP_VECTOR< void* >* stateVec );

  private:
    struct Block
    {
      Block* nextBlock;
      PathNode pathNode[1];
    };

    unsigned Hash( void* voidval );
    unsigned HashSize() const { return 1<<hashShift; }
    unsigned HashMask() const { return ((1<<hashShift)-1); }
    void AddPathNode( unsigned key, PathNode* p );
    Block* NewBlock();
    PathNode* Alloc();

    PathNode**  hashTable;
    Block*    firstBlock;
    Block*    blocks;

    NodeCost* cache;
    int     cacheCap;
    int     cacheSize;

    PathNode  freeMemSentinel;
    unsigned  allocate;       // how big a block of pathnodes to allocate at once
    unsigned  nAllocated;       // number of pathnodes allocated (from Alloc())
    unsigned  nAvailable;       // number available for allocation

    unsigned  hashShift;
    unsigned  totalCollide;
  };


  /* Used to cache results of paths. Much, much faster
     to return an existing solution than to calculate
     a new one. A post on this is here: http://grinninglizard.com/altera/programming/a-path-caching-2/
  */
  class PathCache
  {
  public:
    struct Item {
      // The key:
      void* start;
      void* end;

      bool KeyEqual( const Item& item ) const { return start == item.start && end == item.end; }
      bool Empty() const            { return start == 0 && end == 0; }

      // Data:
      void* next;
      float cost; // from 'start' to 'next'. FLT_MAX if unsolveable.

      unsigned Hash() const {
        const unsigned char *p = (const unsigned char *)(&start);
        unsigned int h = 2166136261U;

        for( unsigned i=0; i<sizeof(void*)*2; ++i, ++p ) {
          h ^= *p;
          h *= 16777619;
        }
        return h;
      }
    };

    PathCache( int itemsToAllocate );
    ~PathCache();

    void Reset();
    void Add( const MP_VECTOR< void* >& path, const MP_VECTOR< float >& cost );
    void AddNoSolution( void* end, void* states[], int count );
    int Solve( void* startState, void* endState, MP_VECTOR< void* >* path, float* totalCost );

    int AllocatedBytes() const { return allocated * sizeof(Item); }
    int UsedBytes() const { return nItems * sizeof(Item); }

    int hit;
    int miss;

  private:
    void AddItem( const Item& item );
    const Item* Find( void* start, void* end );

    Item* mem;
    int   allocated;
    int   nItems;
  };

  struct CacheData {
    CacheData() : nBytesAllocated(0), nBytesUsed(0), memoryFraction(0), hit(0), miss(0), hitFraction(0) {}
    int nBytesAllocated;
    int nBytesUsed;
    float memoryFraction;

    int hit;
    int miss;
    float hitFraction;
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

      // internal
      NOT_CACHED
    };

    /**
      Construct the pather, passing a pointer to the object that implements
      the Graph callbacks.

      @param graph    The "map" that implements the Graph callbacks.
      @param allocate   How many states should be internally allocated at a time. This
                can be hard to get correct. The higher the value, the more memory
                MicroPather will use.
                - If you have a small map (a few thousand states?) it may make sense
                  to pass in the maximum value. This will cache everything, and MicroPather
                  will only need one main memory allocation. For a chess board, allocate
                  would be set to 8x8 (64)
                - If your map is large, something like 1/4 the number of possible
                  states is good.
                  - If your state space is huge, use a multiple (5-10x) of the normal
                  path. "Occasionally" call Reset() to free unused memory.
      @param typicalAdjacent  Used to determine cache size. The typical number of adjacent states
                  to a given state. (On a chessboard, 8.) Higher values use a little
                  more memory.
      @param cache    Turn on path caching. Uses more memory (yet again) but at a huge speed
                advantage if you may call the pather with the same path or sub-path, which
                is common for pathing over maps in games.
    */
    MicroPather( Graph* graph, unsigned allocate = 250, unsigned typicalAdjacent=6, bool cache=true );
    ~MicroPather();

    /**
      Solve for the path from start to end.

      @param startState Input, the starting state for the path.
      @param endState   Input, the ending state for the path.
      @param path     Output, a vector of states that define the path. Empty if not found.
      @param totalCost  Output, the cost of the path, if found.
      @return       Success or failure, expressed as SOLVED, NO_SOLUTION, or START_END_SAME.
    */
    int Solve( void* startState, void* endState, MP_VECTOR< void* >* path, float* totalCost );

    /**
      Find all the states within a given cost from startState.

      @param startState Input, the starting state for the path.
      @param near     All the states within 'maxCost' of 'startState', and cost to that state.
      @param maxCost    Input, the maximum cost that will be returned. (Higher values return
                larger 'near' sets and take more time to compute.)
      @return       Success or failure, expressed as SOLVED or NO_SOLUTION.
    */
    int SolveForNearStates( void* startState, MP_VECTOR< StateCost >* near, float maxCost );

    /** Should be called whenever the cost between states or the connection between states changes.
      Also frees overhead memory used by MicroPather, and calling will free excess memory.
    */
    void Reset();

    // Debugging function to return all states that were used by the last "solve"
    void StatesInPool( MP_VECTOR< void* >* stateVec );
    void GetCacheData( CacheData* data );

    private:
    MicroPather( const MicroPather& );  // undefined and unsupported
    void operator=( const MicroPather ); // undefined and unsupported

    void GoalReached( PathNode* node, void* start, void* end, MP_VECTOR< void* > *path );

    void GetNodeNeighbors(  PathNode* node, MP_VECTOR< NodeCost >* neighborNode );

    #ifdef DEBUG
    //void DumpStats();
    #endif

    PathNodePool      pathNodePool;
    MP_VECTOR< StateCost >  stateCostVec; // local to Solve, but put here to reduce memory allocation
    MP_VECTOR< NodeCost > nodeCostVec;  // local to Solve, but put here to reduce memory allocation
    MP_VECTOR< float >    costVec;

    Graph* graph;
    unsigned frame;           // incremented with every solve, used to determine if cached data needs to be refreshed
    PathCache* pathCache;
  };
};  // namespace grinliz

#endif



#if 0
MicroPather
===========

MicroPather is a path finder and A* solver (astar or a-star) written in platform
independent C++ that can be easily integrated into existing code. MicroPather
focuses on being a path finding engine for video games but is a generic A* solver.
MicroPather is open source, with a license suitable for open source or commercial
use.

The goals of MicroPather are:
* Easy integration into games and other software
* Easy to use and simple interface
* Fast enough

Demo
----

MicroPather comes with a demo application - dungeon.cpp - to show off pathing.
It''s ASCII art dungeon exploring at its finest.

The demo shows an ASCII art dungeon. You can move around by typing a new location, and it will
print the path to that location. In the screen shot above, the path starts in
the upper left corner, and steps to the 'i' at about the middle of
the screen avoiding ASCII walls on the way. The numbers show the path from 0
to 9 then back to 0.

You can even open and close the doors to change possible paths. 'd' at the command prompt.

A Windows Visual C++ 2010 project file and a Linux Makefile are provided. Building it
for another environment is trivial: just compile dungeon.cpp, micropather.h, and
micropather.cpp to a command line app in your environment.

About A*
--------
In video games, the pathfinding problem comes up in many modern games. What
is the shortest distance from point A to point B? Humans are good at that problem
- you pathfind naturally almost every time you move - but tricky to express
as a computer algorithm. A* is the workhorse technique to solve pathing. It
is directed, meaning that it is optimized to find a solution quickly rather
than by brute force, but it will never fail to find a solution if there is one.

A* is much more universal that just pathfinding. A* and MicroPather could be
used to find the solution to general state problems, for example it could be
used to solve for a rubiks cube puzzle.

Terminology
-----------

The *Graph* is the search space. For the pathfinding problem,
this is your Map. It can be any kind of map: squares like the dungeon example,
polygonal, 3D, hexagons, etc.

In pathfinding, a *State* is just a position on the Map. In
the demo, the player starts at State (0,0). Adjacent states are very important,
as you might image. If something is at state (1,1) in the dungeon example,
it has 8 adjacent states (0,1), (2,1) etc. it can move to. Why State instead
of location or node? The terminology comes from the more general application.
The states of a cube puzzle aren''t locations, for example.

States are separated by *Cost*. For simple pathfinding in
the dungeon, the *Cost* is simply distance. The cost from state
(0,0) to (1,0) is 1.0, and the cost from (0,0) to (1,1) is sqrt(2), about
1.4. *Cost* is challenging and interesting because it can be
distance, time, or difficulty.
* using distance as the cost will give the shortest length path
* using traversal time as the cost will give the fastest path
* using difficulty as the cost will give the easiest path
etc.

More info: http://www-cs-students.stanford.edu/~amitp/gameprog.html#paths

Integrating MicroPather Into Your Code
--------------------------------------
Nothing could by simpler! Or at least that''s the goal. More importantly, none
of your game data structures need to change to use MicroPather. The steps, in
brief, are:

1. Include MicroPather files</p>
2. Implement the Graph interface</p>
3. Call the Solver</p>

*Include files*

There are only 2 files for micropather: micropather.cpp and micropather.h.
So there''s no build, no make, just add the 2 files to your project. That''s it.
They are standard C++ and don''t require exceptions or RTTI. (I know, a bunch
of you like exceptions and RTTI. But it does make it less portable and slightly
slower to use them.)

Assuming you build a debug version of your project with _DEBUG or DEBUG (and
everyone does) MicroPather will run extra checking in these modes.

*Implement Graph Interface*

You have some class called Game, or Map, or World, that organizes and stores
your game map. This object (we''ll call it Map) needs to inherit from the abstract
class Graph:

  class Map : public Graph

Graph is pure abstract, so your map class won''t be changed by it (except for
possibly gaining a vtable), or have strange conflicts.
Before getting to the methods of Graph, lets think states, as in:

  void Foo( void* state )

The state pointer is provided by you, the game programmer. What it is? It is
a unique id for a state. For something like a 3D terrain map, like Lilith3D
uses, the states are pointers to a map structure, a 'QuadNode' in
this case. So the state would simply be:

  void* state = (void*) quadNode;

On the other hand, the Dungeon example doesn''t have an object per map location,
just an x and y. It then uses:

  void* state = (void*)( y * MAPX + x );

The state can be anything you want, as long as it is unique and you can convert
to it and from it.

Now, the methods of Graph.

  /**
    Return the least possible cost between 2 states. For example, if your pathfinding
    is based on distance, this is simply the straight distance between 2 points on the
    map. If you pathfinding is based on minimum time, it is the minimal travel time
    between 2 points given the best possible terrain.
  */
  virtual float LeastCostEstimate( void* stateStart, void* stateEnd ) = 0;

  /**
    Return the exact cost from the given state to all its neighboring states. This
    may be called multiple times, or cached by the solver. It *must* return the same
    exact values for every call to MicroPather::Solve(). It should generally be a simple,
    fast function with no callbacks into the pather.
  */
  virtual void AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent ) = 0;

  /**
    This function is only used in DEBUG mode - it dumps output to stdout. Since void*
    aren't really human readable, normally you print out some concise info (like "(1,2)")
    without an ending newline.
  */
  virtual void  PrintStateInfo( void* state ) = 0;

*Call the Solver*

  MicroPather* pather = new MicroPather( myGraph ); // Although you really should set the default params for your game.

  micropather::MPVector< void* > path;
  float totalCost = 0;
  int result = pather->Solve( startState, endState, &path, &totalCost );

That''s it. Given the start state and the end state, the sequence of states
from start to end will be written to the vector.

MicroPather does a lot of caching. You want to create one and keep in around.
It will cache lots of information about your graph, and get faster as it is
called. However, for caching to work, the connections between states and the
costs of those connections must stay the same. (Else the cache information will
be invalid.) If costs between connections does change, be sure to call Reset().

  pather->Reset();

Reset() is a fast call if it doesn''t need to do anything.

Future Improvements and Social Coding
-------------------------------------

I really like getting patches, improvements, and performance enhancements.
Some guidelines:
* Pull requests are the best way to send a change.
* The "ease of use" goal is important to this project. It can be sped up by
  deeper integration into the client code (all states must subclass the State
  object, for example) but that dramatically reduces usability.

Thanks for checking out MicroPather!

Lee Thomason
#endif
