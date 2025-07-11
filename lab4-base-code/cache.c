#include "cache.h"
#include "dogfault.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// DO NOT MODIFY THIS FILE. INVOKE AFTER EACH ACCESS FROM runTrace
void print_result(result r) {
  if (r.status == CACHE_EVICT)
    printf(" [status: miss eviction, victim_block: 0x%llx, insert_block: 0x%llx]",
           r.victim_block_addr, r.insert_block_addr);
  if (r.status == CACHE_HIT)
    printf(" [status: hit]");
  if (r.status == CACHE_MISS)
    printf(" [status: miss, insert_block: 0x%llx]", r.insert_block_addr);
}

/* This is the entry point to operate the cache for a given address in the trace file.
 * First, is increments the global lru_clock in the corresponding cache set for the address.
 * Second, it checks if the address is already in the cache using the "probe_cache" function.
 * If yes, it is a cache hit:
 *     1) call the "hit_cacheline" function to update the counters inside the hit cache 
 *        line, including its lru_clock and access_counter.
 *     2) record a hit status in the return "result" struct and update hit_count 
 * Otherwise, it is a cache miss:
 *     1) call the "insert_cacheline" function, trying to find an empty cache line in the
 *        cache set and insert the address into the empty line. 
 *     2) if the "insert_cacheline" function returns true, record a miss status and the
          inserted block address in the return "result" struct and update miss_count
 *     3) otherwise, if the "insert_cacheline" function returns false:
 *          a) call the "victim_cacheline" function to figure which victim cache line to 
 *             replace based on the cache replacement policy (LRU and LFU).
 *          b) call the "replace_cacheline" function to replace the victim cache line with
 *             the new cache line to insert.
 *          c) record an eviction status, the victim block address, and the inserted block
 *             address in the return "result" struct. Update miss_count and eviction_count.
 */

// Student 1
result operateCache(const unsigned long long address, Cache *cache) {
  set->lru_clock++; //global lru clock
  if (probe_cache(address, cache)) { // If cache is found and to be true, updates hit_cacheline function, returns hit status and upcounts hit_count
    hit_cacheline(address, cache);
    r.status = 1;
    cache->hit_count += 1;
  else { // If false, tries to find an empty cache line in the cache set
    if (insert_cacheline(address, cache)) {
      r.status = 0;
      cache->miss_count += 1;
    else {
      r.victim_block_addr = address; // record victim block address
      replace_cacheline(address, victim_cacheline(address, cache), cache); // replaces block address
      cache->miss_count += 1; //ups miss count
      cache->eviction_count += 1;  // ups eviction count
      r.status = 2; //record eviction status
      r.insert_block_addr = address; //records insert block address
        }
      }
    }
  }
  return r;
}

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE
// Given an address, return the block (aligned) address,
// i.e., byte offset bits are cleared to 0

// Student 1
unsigned long long address_to_block(const unsigned long long address, const Cache *cache) {
  return ((address >> cache->blockBits) << cache->blockBits);
}

// Return the cache tag of an address

// Student 2
unsigned long long cache_tag(const unsigned long long address, const Cache *cache) {
  /* YOUR CODE HERE */
  return 0;
}

// Return the cache set index of the address

// Student 1
unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
  int mask = (1 << cache->setBits) - 1; //set out a mask with 2^s 1's, such that we can make 2^s spots
  return((address >> cache->blockBits) & mask); //Move the offset, then mask the next s bits
}

// Check if the address is found in the cache. If so, return true. else return false.

// Student 2
bool probe_cache(const unsigned long long address, const Cache *cache) {
  /* YOUR CODE HERE */
  return false;
}

// Access address in cache. Called only if probe is successful.
// Update the LRU (least recently used) or LFU (least frequently used) counters.

// Student 1 do LRU
// Student 2 do LFU
void hit_cacheline(const unsigned long long address, Cache *cache){
  Set *set = &cache->sets[cache_set(address, cache)];
  unsigned long long localTag = cache_tag(address, cache);

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &set->lines[i];
    if (line->valid && line->tag = localTag) {
      line->lru_clock = set->lru_clock;
      line->access_counter++;
      return;
    }
  }
  return;
 }

/* This function is only called if probe_cache returns false, i.e., the address is
 * not in the cache. In this function, it will try to find an empty (i.e., invalid)
 * cache line for the address to insert. 
 * If it found an empty one:
 *     1) it inserts the address into that cache line (marking it valid).
 *     2) it updates the cache line's lru_clock based on the global lru_clock 
 *        in the cache set and initiates the cache line's access_counter.
 *     3) it returns true.
 * Otherwise, it returns false.  
 */ 

// Student 1 - LRU state
// Student 2 - LFU state
bool insert_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long localTag = cache_tag(address, cache);
  unsigned long long localsetIndx = cache_set(address, cache);
  unsigned long long localBlock = address_to_block(address, cache);
  Set *set = cache->sets[localsetIndx];

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = sets->line[i];
    if (!(lines->valid)) {
      line->block_addr = localBlock;
      line->valid = true;
      line->tag = localTag;
      line->lru_clock = set->lru_clock
      line->access_counter += 1;

      return true;
    }
  }
  return false;
}

// If there is no empty cacheline, this method figures out which cacheline to replace
// depending on the cache replacement policy (LRU and LFU). It returns the block address
// of the victim cacheline; note we no longer have access to the full address of the victim

// Student 1 - implement LRU section
// Student 2 - implement LFU section
unsigned long long victim_cacheline(const unsigned long long address, const Cache *cache) {
  if (line.lru_clock < line.access_counter) {
    //LRU Policy

  } else if (line.lru_clock > line.access_counter) {
    //LFU Policy

  } else {
    // idk
  }
  return 0;
}

/* Replace the victim cacheline with the new address to insert. Note for the victim cachline,
 * we only have its block address. For the new address to be inserted, we have its full address.
 * Remember to update the new cache line's lru_clock based on the global lru_clock in the cache
 * set and initiate the cache line's access_counter.
 */
// Student 1 - update LRU clock
// Student 2 - implement, update LFU clock
void replace_cacheline(const unsigned long long victim_block_addr, const unsigned long long insert_addr, Cache *cache) {
  /* YOUR CODE HERE */
}

// allocate the memory space for the cache with the given cache parameters
// and initialize the cache sets and lines.
// Initialize the cache name to the given name

// Student 1 - allocation for cache sets and lines
void cacheSetUp(Cache *cache, char *name) {
  cache->hit_count = 0;
  cache->miss_count = 0;
  cache->eviction_count = 0;
  int Sets = 1 - cache->setBits;

  cache->sets = malloc(sizeof(Set) * Sets); //Allocate the memory for the sets

  for (int i = 0; i < Sets; i++) {
    Set *set = &cache->sets[i];
    set->lines = malloc(sizeof(Line) * cache->linesPerSet); //Allocate memory for each lines

    set->lru_clock = 0;

    for (int j = 0; j < cache->linesPerSet; j++) {
      Line *line = &set->lines[j];
      line->valid = false;
      line->tag = 0;
      line->block_addr = 0;
      line->lru_clock = 0;
      line->access_counter = 0;
    }
  }

  cache->name = name;
}

// deallocate the memory space for the cache

// Student 2
void deallocate(Cache *cache) {
  /* YOUR CODE HERE */
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}
