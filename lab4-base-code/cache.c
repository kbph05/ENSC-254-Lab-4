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
  Set *set = &cache->sets[cache_set(address, cache)];
  set->lru_clock += 1; //global lru clock
  result r; // result status of hit or miss

  if (probe_cache(address, cache) == true) { // If cache is found and to be true, updates hit_cacheline function, returns hit status and upcounts hit_count
    hit_cacheline(address, cache);
    r.status = CACHE_HIT;
    cache->hit_count += 1;
    return r;
  }
  else { // If false, tries to find an empty cache line in the cache set
    if (insert_cacheline(address, cache) == true) { // if insert in cacheline is true, status is cache_miss and add 1 to miss count
      r.status = CACHE_MISS;
      r.insert_block_addr = address_to_block(address, cache);
      cache->miss_count += 1;
      return r;
    }
    else {
      unsigned long long victim = victim_cacheline(address, cache);
      replace_cacheline(victim, address, cache); // replaces block address
      r.status = CACHE_EVICT; // record eviction status
      r.victim_block_addr = victim;
      r.insert_block_addr = address_to_block(address, cache);
      cache->eviction_count += 1;  // adds 1 eviction count
      cache->miss_count += 1;
      return r;
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
  return ((address >> (cache->blockBits + cache->setBits)));
}

// Return the cache set index of the address
// Student 2
unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
  return ((address >> cache->blockBits) & ((1 << cache->setBits) - 1)); // return address masked at the last s bits
}

// Check if the address is found in the cache. If so, return true. else return false.
// Student 2
bool probe_cache(const unsigned long long address, const Cache *cache) {
  unsigned long long localTag = cache_tag(address, cache);
  Set *set = &cache->sets[cache_set(address, cache)]; // get the set that the address is in

  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && (set->lines[i].tag == localTag)) {
      return true;
    }
  }
  return false;
}

// Access address in cache. Called only if probe is successful.
// Update the LRU (least recently used) or LFU (least frequently used) counters.
// Student 1
void hit_cacheline(const unsigned long long address, Cache *cache){
  Set *set = &cache->sets[cache_set(address, cache)];
  unsigned long long localTag = cache_tag(address, cache);

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &set->lines[i];
    if (line->valid && (line->tag == cache_tag(address, cache))) {
      line->lru_clock = set->lru_clock;
      line->access_counter += 1;
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

// Student 1
bool insert_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long localTag = cache_tag(address, cache);
  unsigned long long localsetIndx = cache_set(address, cache);
  unsigned long long localBlock = address_to_block(address, cache);
  Set *set = &cache->sets[localsetIndx];

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &set->lines[i];
    if (!(set->lines[i].valid)) {
      set->lines[i].block_addr = localBlock;
      set->lines[i].valid = true; 
      set->lines[i].tag = localTag;
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].access_counter = 1;
      return true;
    }
  }
  return false;
}

// If there is no empty cacheline, this method figures out which cacheline to replace
// depending on the cache replacement policy (LRU and LFU). It returns the block address
// of the victim cacheline; note we no longer have access to the full address of the victim

// Student 2
unsigned long long victim_cacheline(const unsigned long long address, const Cache *cache) {
  Set *set = &cache->sets[cache_set(address, cache)]; // select the set with the address of the victim
  Line *victim = &set->lines[0]; // set as the current "best" victim

  if (cache->lfu == 0) {
    for (int i = 0; i < cache->linesPerSet; i++) {
      if (set->lines[i].valid && (set->lines[i].lru_clock < victim->lru_clock)) {
        // LRU policy
        victim = &set->lines[i]; // if the value of the lru_clock of next line is smaller, set that as the victim
      }
    }
  }

  if (cache->lfu == 1) {
    for (int i = 0; i < cache->linesPerSet; i++) {
      if (set->lines[i].valid && (set->lines[i].access_counter < victim->access_counter)) {
        // LFU policy
        victim = &set->lines[i]; // if the value of the access_counter of next line is smaller, set that as the victim
      }
      else if (set->lines[i].access_counter == victim->access_counter) {
        // if access counter is equal to victims use LRU policy to fix
        if (set->lines[i].valid && (set->lines[i].lru_clock < victim->lru_clock)) {
          victim = &set->lines[i];
          continue;
        }
      }
    }
  }
  return victim->block_addr;
}


/* Replace the victim cacheline with the new address to insert. Note for the victim cachline,
 * we only have its block address. For the new address to be inserted, we have its full address.
 * Remember to update the new cache line's lru_clock based on the global lru_clock in the cache
 * set and initiate the cache line's access_counter.
 */
// Student 2
void replace_cacheline(const unsigned long long victim_block_addr, const unsigned long long insert_addr, Cache *cache) {
  Set *set = &cache->sets[cache_set(insert_addr, cache)];

  for (int i = 0; i < cache->linesPerSet; i++) {
    if ((set->lines[i].valid) && set->lines[i].block_addr == victim_block_addr) {
      set->lines[i].block_addr = address_to_block(insert_addr, cache);
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].tag = cache_tag(insert_addr, cache);
      set->lines[i].access_counter = 1; // re init the lfu access_counter
      break;
    } 
  }
}

// allocate the memory space for the cache with the given cache parameters
// and initialize the cache sets and lines.
// Initialize the cache name to the given name

// Student 1 - allocation for cache sets and lines
void cacheSetUp(Cache *cache, char *name) {
  cache->hit_count = 0;
  cache->miss_count = 0;
  cache->eviction_count = 0;
  int s = 1 << cache->setBits;


  cache->sets = malloc(sizeof(Set) * s); //Allocate the memory for the sets

  for (int i = 0; i < s; i++) {
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
  for (int i = 0; i < (1 << cache->setBits); i++) { // number of sets per cache = 2 * setBits
      free(cache->sets[i].lines); // free all the lines in each set
  }
  // free all sets in cache:
  free(cache->sets);
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}
