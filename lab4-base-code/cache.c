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
  cache->sets->lru_clock += 1; //global lru clock
  result r; // result status of hit or miss (i guess they dont miss huh...sorry ill delete this later)
  if (probe_cache(address, cache)) { // If cache is found and to be true, updates hit_cacheline function, returns hit status and upcounts hit_count
    hit_cacheline(address, cache);
    r.status = 1;
    cache->hit_count += 1;
  }
  else { // If false, tries to find an empty cache line in the cache set
    if (insert_cacheline(address, cache)) {
      r.status = 0;
      cache->miss_count += 1;
    }
    else {
      r.victim_block_addr = address_to_block(address, cache); // record victim block address
      replace_cacheline(address, victim_cacheline(address, cache), cache); // replaces block address
      cache->miss_count += 1; //ups miss count
      cache->eviction_count += 1;  // ups eviction count
      r.status = 2; //record eviction status
      r.insert_block_addr = address; //records insert block address
    }
  }
  return r;
}

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE
// Given an address, return the block (aligned) address,
// i.e., byte offset bits are cleared to 0
// Student 1
unsigned long long address_to_block(const unsigned long long address, const Cache *cache) {
  //return ((address << cache->blockBits) >> cache->blockBits);
  return (address & ((1 << cache->blockBits) - 1)); // return address with a mask at the last b (blockBits) bits
}

// Return the cache tag of an address
// Student 2
unsigned long long cache_tag(const unsigned long long address, const Cache *cache) {
  return (address >> (cache->blockBits + cache->setBits));
}

// Return the cache set index of the address
// Student 2
unsigned long long cache_set(const unsigned long long address, const Cache *cache) {
  return ((address >> cache->blockBits) & ((1 << cache->setBits) - 1)); // return address sifted right by the blockBits amount and then masked at the last s bits
}

// Check if the address is found in the cache. If so, return true. else return false.
// Student 2
bool probe_cache(const unsigned long long address, const Cache *cache) {
  Set *set = &cache->sets[cache_set(address, cache)]; // get the set that the address is in

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &set->lines[i];
    if (address_to_block(address, cache) == line->block_addr) {
      return true;
    }
    else {
      return false;
    }
  }
}

// Access address in cache. Called only if probe is successful.
// Update the LRU (least recently used) or LFU (least frequently used) counters.

// Student 1
void hit_cacheline(const unsigned long long address, Cache *cache){
  Set *set = &cache->sets[cache_set(address, cache)];
  unsigned long long localTag = cache_tag(address, cache);

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &set->lines[i];
    if (line->valid && (line->tag == localTag)) {
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

// Student 1
bool insert_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long localTag = cache_tag(address, cache);
  unsigned long long localsetIndx = cache_set(address, cache);
  unsigned long long localBlock = address_to_block(address, cache);
  Set set = cache->sets[localsetIndx];

  for (int i = 0; i < cache->linesPerSet; i++) {
    Line *line = &cache->sets->lines[i];
    if (!(line->valid)) {
      line->block_addr = localBlock;
      line->valid = true; 
      line->tag = localTag;
      line->lru_clock = set.lru_clock;
      line->access_counter += 1;
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

  Line *line;
  for (int i = 0; i < cache->setBits; i++) {
    for (int j = 0; j < cache->linesPerSet; j++) {
      line = &cache->sets[i].lines[j]; // access the block address in the cache line
      if (line->lru_clock < line->access_counter) {
        return line->block_addr;
      }
      else if (line->lru_clock > line->access_counter) {
        return line->block_addr;
      }
    }
  }

  // if (cache->sets->lines->lru_clock < cache->sets->lines->access_counter) {
  //   //LRU Policy
    
  // } else if (cache->sets->lines->lru_clock > cache->sets->lines->access_counter) {
  //   //LFU Policy

  // } else {
  //   // idk
  //   printf("%s", "");
  // }
  return 0;
}

/* Replace the victim cacheline with the new address to insert. Note for the victim cachline,
 * we only have its block address. For the new address to be inserted, we have its full address.
 * Remember to update the new cache line's lru_clock based on the global lru_clock in the cache
 * set and initiate the cache line's access_counter.
 */
// Student 2
void replace_cacheline(const unsigned long long victim_block_addr, const unsigned long long insert_addr, Cache *cache) {
  /* YOUR CODE HERE */
  
  Line *line;
  for (int i = 0; i < cache->setBits; i++) {
    for (int j = 0; j < cache->linesPerSet; j++) {
    line = &cache->sets[i].lines[j]; // access the block address in the cache line
    if (line->block_addr == victim_block_addr) {
      line->block_addr = address_to_block(insert_addr, cache);
      line->lru_clock = cache->sets->lru_clock;
      line->tag = cache_tag(insert_addr, cache);
      line->access_counter+= 1;
    } 
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
  /* YOUR CODE HERE */
  free(cache->sets);
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}
