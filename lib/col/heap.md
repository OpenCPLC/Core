## Heap

This library provides a lightweight memory manager designed for embedded projects.
It replaces the need for system malloc/free with a predictable and portable solution.





Garbage-collector stack – a simple helper that tracks allocations per thread.
Memory requested with heap_new is automatically registered in a thread-local stack.
Calling heap_clear frees all tracked allocations at once, making cleanup easy and fast.

The goal is to keep code small and safe while fitting real-time and resource-limited environments.

---

## Features

Heap module implements static memory partitioning over a fixed-size buffer; no dynamic system allocation occurs after startup.

### Basic heap
- `heap_init()` - Initialize heap memory. Call once at startup.  
- `heap_alloc(size)`  
  Allocate a block of memory. Returns pointer or `NULL`.  

- `heap_free(ptr)`  
  Free a block. Safe to call with `NULL`.  

- `heap_reloc(ptr, size)`  
  Resize a block. Can grow or shrink.  
  - If `ptr == NULL` → same as `heap_alloc()`.  
  - If `size == 0` → frees block and returns `NULL`.  
  - If smaller → keeps same block.  
  - If larger → moves to new block and copies data.  

### Garbage-collector stack
- `heap_new(size)`  
  Allocate memory tracked by GC. Each thread has its own stack.  
  If the stack is full, it expands by `HEAP_NEW_BLOCK`.  

- `heap_clear()`  
  Free all memory allocated with `heap_new()` for the active thread.  

---




## Configuration

- `HEAP_SIZE` – total heap size in bytes (default 8192).  
- `HEAP_ALIGN` – memory alignment (default 8).  
- `HEAP_NEW_BLOCK` – number of slots added to GC stack when it expands (default 16).  
- `VRTS_SWITCHING` – if enabled, each thread in VRTS gets its own GC stack.  

---

## Example

Two layers are available:

Heap functions – basic memory management: `heap_alloc`, `heap_free`, `heap_reloc`.
They give direct control and work like standard C functions, but with alignment and safety suitable for microcontrollers.

```c
heap_init();
// Basics usage
void *a = heap_alloc(32);
a = heap_reloc(a, 64);
heap_free(a);
```

```c
heap_init();
// Garbage-collector stack
void *b1 = heap_new(24);
void *b2 = heap_new(36);
heap_clear(); // frees b1 and b2
```



