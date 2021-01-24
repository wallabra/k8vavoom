// static inline bool BinHeapLess (const type &a, const type &b) noexcept { return (a < b); }


// ////////////////////////////////////////////////////////////////////////// //
// `clearEmptyItems`: clear removed structs with `=T.init`?
template <typename T> class TBinHeap_Class_Name {
private:
  T *elem;
  size_t elemUsed;
  size_t elemSize;

private:
  inline bool lessfn (size_t i0, size_t i1) const noexcept {
    if (i0 == i1) return false;
    return BinHeapLess(elem[i0], elem[i1]);
  }

  inline bool lessfn (const T &v0, size_t i1) const noexcept {
    return BinHeapLess(v0, elem[i1]);
  }

private:
  void heapify (size_t root) noexcept {
    for (;;) {
      size_t smallest = 2*root+1; // left child
      if (smallest >= elemUsed) break; // anyway
      const size_t right = smallest+1; // right child
      if (!lessfn(smallest, root)) smallest = root;
      if (right < elemUsed && lessfn(right, smallest)) smallest = right;
      if (smallest == root) break;
      { // swap
        auto tmp = elem[root];
        elem[root] = elem[smallest];
        elem[smallest] = tmp;
      }
      root = smallest;
    }
  }

public:
  TBinHeap_Class_Name () : elem(nullptr), elemUsed(0), elemSize(0) {}
  TBinHeap_Class_Name (const T *ainit, size_t acount) : elem(nullptr), elemUsed(0), elemSize(0) { build(ainit, acount); }
  TBinHeap_Class_Name (const TBinHeap_Class_Name &) = delete;

  TBinHeap_Class_Name &operator = (const TBinHeap_Class_Name &) = delete;

  void reset () noexcept {
    #ifndef VV_HEAP_SKIP_DTOR
    T *el = elem;
    for (size_t idx = elemUsed; idx--; ++el) el->~T();
    #endif
    elemUsed = 0;
  }

  inline void resetNoDtor () noexcept {
    elemUsed = 0;
  }

  inline void clear () noexcept {
    reset();
    if (elem) { Z_Free(elem); elem = nullptr; }
    elemUsed = elemSize = 0;
  }

  inline bool isEmpty () const noexcept { return (elemUsed == 0); }
  size_t length () const noexcept { return elemUsed; }

  void push (const T &val) noexcept {
    //if (elemUsed == size_t.max-2) assert(0, "too many elements in heap");
    size_t i = elemUsed;
    if (i == elemSize) {
      size_t newSize = elemSize+(elemSize/3+64);
      elem = (T *)Z_Realloc(elem, newSize*sizeof(T));
      memset((void *)(elem+elemSize), 0, (newSize-elemSize)*sizeof(T));
      elemSize = newSize;
    }
    ++elemUsed;
    while (i != 0) {
      size_t par = (i-1)/2; // parent
      if (!lessfn(val, par)) break;
      elem[i] = elem[par];
      i = par;
    }
    elem[i] = val;
  }

  inline const T &peek () const noexcept { return elem[0]; }
  inline T &peek () noexcept { return elem[0]; }

  void popFront () noexcept {
    if (elemUsed > 1) {
      #ifndef VV_HEAP_SKIP_DTOR
      elem[0].~T();
      #endif
      elem[0] = elem[--elemUsed];
      heapify(0);
    } else if (elemUsed == 1) {
      #ifndef VV_HEAP_SKIP_DTOR
      elem[0].~T();
      #endif
      elemUsed = 0;
    }
  }

  inline T pop () noexcept {
    auto res = elem[0];
    popFront();
    return res;
  }

  // this will copy `arr`
  void build (const T *arr, size_t acount) noexcept {
    if (acount == 0) { clear(); return; }
    reset();
    if (elemSize < acount || elemSize-acount >= acount) {
      elem = (T *)Z_Realloc(elem, acount*sizeof(T));
      if (elemSize < acount) memset((void *)(elem+elemSize), 0, (acount-elemSize)*sizeof(T));
      elemSize = acount;
    }
    elemUsed = acount;
    for (size_t n = 0; n < acount; ++n) elem[n] = arr[n];
    // making sure that heap property satisfied; loop from last parent up to (and including) first item
    for (size_t par = (elemUsed-1)/2+1; par--; ) heapify(par);
  }
};
