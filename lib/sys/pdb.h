// lib/sys/pdb.h

#ifndef PDB_H_
#define PDB_H_

#include <stdbool.h>
#include <stdint.h>
#include "xdef.h"
#include "log.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef PDB_LOG
  // Log function for `PDB` messages
  #define PDB_LOG(fmt, ...) LOG_TAG_DBG("pdb", fmt, ##__VA_ARGS__)
#endif

#ifndef PDB_RECORD_LIMIT
  // Largest record [B]: a copy in `PDB_Iter_t` and on the stack of `PDB_Init` and `PDB_Insert`
  #define PDB_RECORD_LIMIT 32
#endif

#ifndef PDB_QUERY
  // Number range, filter, paging, `PDB_Select` and `PDB_Count`; off leaves the core,
  // what a bootloader needs: init, insert, delete and a walk in either direction.
  #define PDB_QUERY 1
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  PDB_Desc = 0,  // newest first
  PDB_Asc = 1    // oldest first
} PDB_Dir_t;

#if(PDB_QUERY)
// Record filter, `true` keeps the record, a copy in RAM; `ctx` is `PDB_Query_t.filter_ctx`
typedef bool (*PDB_Filter_t)(const void *record, void *ctx);
#endif

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Pico database: records of one size over flash pages, each page opened by its header.
 *   A record is the caller's struct: its first 4B the number PDB gives it, from `1`,
 *   its last 4B the CRC-32 PDB writes last.
 *   A ring erases its oldest page when the newest fills, `no_wrap` refuses a record then.
 *   A page under a header of another format, kind or record size is left alone.
 *   Not reentrant: one thread, or the cooperative scheduler.
 * @param[in] page_start First page of the base
 * @param[in] page_count Pages of the base, `1` or more for `no_wrap`, `2` or more for a ring
 * @param[in] record_size Record [B], a multiple of `8` from `16` to `PDB_RECORD_LIMIT`
 * @param[in] kind Base kind, in every page header
 * @param[in] no_wrap No ring: a full base refuses a record
 * Internal:
 * @param _page Newest page, `UINT16_MAX` while the base holds none
 * @param _slot Next slot on `_page`, past the last one not erased
 * @param _first Number of the first record of `_page`, from its header
 * @param _next Number of the next record
 * @param _fresh First free slot of `_page` may hold a write cut at its very start
 */
typedef struct {
  uint16_t page_start;
  uint16_t page_count;
  uint16_t record_size;
  uint16_t kind;
  bool no_wrap;
  // internal
  uint16_t _page;
  uint16_t _slot;
  uint32_t _first;
  uint32_t _next;
  bool _fresh;
} PDB_t;

/**
 * @brief Query of a walk, a zero field is no constraint.
 * @param[in] dir `PDB_Desc` newest first, `PDB_Asc` oldest first
 * @param[in] number_min Records numbered `number_min` or more
 * @param[in] number_max Records numbered `number_max` or less
 * @param[in] limit Records to return, `0` = every one
 * @param[in] skip Matching records to skip first, for paging
 * @param[in] filter Record filter, `NULL` = none
 * @param[in] filter_ctx Context passed to `filter`, owned by the caller
 */
typedef struct {
  PDB_Dir_t dir;
  #if(PDB_QUERY)
  uint32_t number_min;
  uint32_t number_max;
  uint32_t limit;
  uint32_t skip;
  PDB_Filter_t filter;
  void *filter_ctx;
  #endif
} PDB_Query_t;

/**
 * @brief Walk over the valid records, one per `PDB_IterNext`, in the order of their numbers.
 *   A record a write took twice, same number, comes once.
 * @param[out] count Records returned so far
 * Internal:
 * @param _pdb Database
 * @param _query Query, copied
 * @param _page Page under the cursor
 * @param _slot Slot under the cursor
 * @param _first Number of the first record of `_page`
 * @param _number Number of the last valid record walked, `0` before the first
 * @param _skipped Matching records skipped so far
 * @param _done Walk finished
 * @param _record Record under the cursor, copied through ECC, aligned for any record struct
 */
typedef struct {
  uint32_t count;
  // internal
  PDB_t *_pdb;
  PDB_Query_t _query;
  uint16_t _page;
  uint16_t _slot;
  uint32_t _first;
  uint32_t _number;
  uint32_t _skipped;
  bool _done;
  uint64_t _record[PDB_RECORD_LIMIT / 8];
} PDB_Iter_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Find the newest page and the next number from the page headers,
 *   then the write cursor on that page.
 *   Reads only: nothing is written or erased.
 * @param[in,out] pdb Database
 * @return `OK` on success, `ERR` on invalid config
 */
status_t PDB_Init(PDB_t *pdb);

/**
 * @brief Add a record, PDB writing its number and CRC-32, then reading it back through ECC.
 *   A full newest page moves the base on to the next page, erased whole and given its header,
 *   in a ring the oldest one.
 *   The first record after `PDB_Init` zeroes the first free slot and goes in the next one.
 *   A record that does not read back clean goes once more, same number, into the next slot.
 * @param[in,out] pdb Database
 * @param[in] record Record of `record_size` bytes, its number and CRC-32 left to PDB
 * @return `OK` on success, `ERR` on a full `no_wrap` base, no page to move on to or a flash error
 */
status_t PDB_Insert(PDB_t *pdb, const void *record);

/**
 * @brief Erase every page of the base, under a header of another kind too,
 *   and start the numbers again from `1`.
 * @param[in,out] pdb Database
 * @return `OK` on success, `ERR` on a flash error
 */
status_t PDB_Delete(PDB_t *pdb);

/**
 * @brief Start a walk over the records matching `query`.
 * @param[in] pdb Database
 * @param[out] iter Iterator, owned by the caller
 * @param[in] query Query, copied into the iterator
 * @return `OK`
 */
status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query);

/**
 * @brief Next matching record.
 * @param[in,out] iter Iterator
 * @param[out] out Room for `record_size` bytes, `NULL` = advance only
 * @return `OK` on a record, `ERR` when the walk is over
 */
status_t PDB_IterNext(PDB_Iter_t *iter, void *out);

/**
 * @brief Record under the iterator, the copy it holds, valid until the next `PDB_IterNext`.
 * @param[in] iter Iterator after a successful `PDB_IterNext`
 * @return Pointer to the record
 */
const void *PDB_IterRef(PDB_Iter_t *iter);

#if(PDB_QUERY)

/**
 * @brief Copy the records matching `query` into a buffer.
 * @param[in] pdb Database
 * @param[in] query Query
 * @param[out] out Output buffer
 * @param[in] max Records that fit in `out`, `0` returns `0`
 * @return Records copied
 */
uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query, void *out, uint32_t max);

/**
 * @brief Count the records matching `query`.
 * @param[in] pdb Database
 * @param[in] query Query
 * @return Matching records
 */
uint32_t PDB_Count(PDB_t *pdb, const PDB_Query_t *query);

#endif
//-------------------------------------------------------------------------------------------------
#endif
