// ECE 430.322: Computer Organization
// Lab 4: Memory System Simulation

/**
 *
 * This is the base cache structure that maintains and updates the tag store
 * depending on a cache hit or a cache miss. Note that the implementation here
 * will be used throughout Lab 4. 
 */

#include "cache_base.h"

#include <cmath>
#include <string>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iomanip>

/**
 * This allocates an "assoc" number of cache entries per a set
 * @param assoc - number of cache entries in a set
 */
cache_set_c::cache_set_c(int assoc) {
  m_entry = new cache_entry_c[assoc];
  m_assoc = assoc;
}

// cache_set_c destructor
cache_set_c::~cache_set_c() {
  delete[] m_entry;
}

void cache_set_c::touch(int index) {
  lru_stack.remove(index);
  lru_stack.push_front(index);
}

int cache_set_c::evict() {
  if (lru_stack.empty()) {
    return -1;
  }
  else{
    int lru_index = lru_stack.back();
    lru_stack.pop_back();
    return lru_index;
  }
}

/**
 * This constructor initializes a cache structure based on the cache parameters.
 * @param name - cache name; use any name you want
 * @param num_sets - number of sets in a cache
 * @param assoc - number of cache entries in a set
 * @param line_size - cache block (line) size in bytes
 *
 * @note You do not have to modify this (other than for debugging purposes).
 */
cache_base_c::cache_base_c(std::string name, int num_sets, int assoc, int line_size) {
  m_name = name;
  m_num_sets = num_sets;
  m_line_size = line_size;

  m_set = new cache_set_c *[m_num_sets];

  for (int ii = 0; ii < m_num_sets; ++ii) {
    m_set[ii] = new cache_set_c(assoc);

    // initialize tag/valid/dirty bits
    for (int jj = 0; jj < assoc; ++jj) {
      m_set[ii]->m_entry[jj].m_valid = false;
      m_set[ii]->m_entry[jj].m_dirty = false;
      m_set[ii]->m_entry[jj].m_tag   = 0;
    }
  }

  // initialize stats
  m_num_accesses = 0;
  m_num_hits = 0;
  m_num_misses = 0;
  m_num_writes = 0;
  m_num_writebacks = 0;
}

// cache_base_c destructor
cache_base_c::~cache_base_c() {
  for (int ii = 0; ii < m_num_sets; ++ii) { delete m_set[ii]; }
  delete[] m_set;
}

/** 
 * This function looks up in the cache for a memory reference.
 * This needs to update all the necessary meta-data (e.g., tag/valid/dirty) 
 * and the cache statistics, depending on a cache hit or a miss.
 * @param address - memory address 
 * @param access_type - read (0), write (1), or instruction fetch (2)
 * @param is_fill - if the access is for a cache fill
 * @param return "true" on a hit; "false" otherwise.
 */
bool cache_base_c::access(addr_t address, int access_type, bool is_fill) {
  ////////////////////////////////////////////////////////////////////
  // TODO: Write the code to implement this function
    int block_offset_bits = log2(m_line_size); // 6
    int set_index_bits = log2(m_num_sets); // 6
    
    addr_t block_offset_mask = (1 << block_offset_bits) - 1; // 63
    addr_t set_index_mask = ((1 << set_index_bits) - 1) << block_offset_bits; // 4032
    
    addr_t set_index = (address & set_index_mask) >> block_offset_bits;
    addr_t tag = address >> (block_offset_bits + set_index_bits);
    
    cache_set_c* cache_set = m_set[set_index];

    bool hit = false;
    int lru_index = -1;
    
    for (int i = 0; i < cache_set->m_assoc; i++) {
        if (cache_set->m_entry[i].m_valid && cache_set->m_entry[i].m_tag == tag) {
            hit = true;
            lru_index = i;
            break;
        }
    }

    m_num_accesses++;
    
    if (hit && is_fill) {
        m_num_hits++;
        if (access_type == WRITE) {
            m_num_writes++;
            cache_set->m_entry[lru_index].m_dirty = true;
        }
        cache_set->touch(lru_index);
        return true;
    } 
    else {
      m_num_misses++;

      for (int i = 0; i < cache_set->m_assoc; i++) {
        if (!cache_set->m_entry[i].m_valid) {
          lru_index = i;
          break;
        }
      }
      if (lru_index == -1) {
        lru_index = cache_set->evict();
      }

      if (cache_set->m_entry[lru_index].m_valid && cache_set->m_entry[lru_index].m_dirty) {
        m_num_writebacks++;
        cache_set->m_entry[lru_index].m_dirty = false;
      }
      cache_set->m_entry[lru_index].m_dirty = (access_type == WRITE);

      if (access_type == WRITE) {
        m_num_writes++;
      }

      cache_set->m_entry[lru_index].m_tag = tag;
      cache_set->m_entry[lru_index].m_valid = true;
      cache_set->touch(lru_index);
      return false;
    }
  ////////////////////////////////////////////////////////////////////
}

/**
 * Print statistics (DO NOT CHANGE)
 */
void cache_base_c::print_stats() {
  std::cout << "------------------------------" << "\n";
  std::cout << m_name << " Hit Rate: "          << (double)m_num_hits/m_num_accesses*100 << " % \n";
  std::cout << "------------------------------" << "\n";
  std::cout << "number of accesses: "    << m_num_accesses << "\n";
  std::cout << "number of hits: "        << m_num_hits << "\n";
  std::cout << "number of misses: "      << m_num_misses << "\n";
  std::cout << "number of writes: "      << m_num_writes << "\n";
  std::cout << "number of writebacks: "  << m_num_writebacks << "\n";
}


/**
 * Dump tag store (for debugging) 
 * Modify this if it does not dump from the MRU to LRU positions in your implementation.
 */
void cache_base_c::dump_tag_store(bool is_file) {
  auto write = [&](std::ostream &os) { 
    os << "------------------------------" << "\n";
    os << m_name << " Tag Store\n";
    os << "------------------------------" << "\n";

    for (int ii = 0; ii < m_num_sets; ii++) {
      for (int jj = 0; jj < m_set[0]->m_assoc; jj++) {
        os << "[" << (int)m_set[ii]->m_entry[jj].m_valid << ", ";
        os << (int)m_set[ii]->m_entry[jj].m_dirty << ", ";
        os << std::setw(10) << std::hex << m_set[ii]->m_entry[jj].m_tag << std::dec << "] ";
      }
      os << "\n";
    }
  };

  if (is_file) {
    std::ofstream ofs(m_name + ".dump");
    write(ofs);
  } else {
    write(std::cout);
  }
}
