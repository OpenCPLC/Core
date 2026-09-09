// hal/stm32/per/pdb.h

#ifndef PDB_H_
#define PDB_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "flash.h"
#include "xdef.h"
#include "crc.h"
#include "log.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef PDB_LOG
  // Log function for `PDB` messages
  #define PDB_LOG(fmt, ...) LOG_TAG_DBG("pdb", fmt, ##__VA_ARGS__)
#endif

#ifndef PDB_RECORD_LIMIT
  // Largest record: payload, CRC and padding to 8 bytes; a stack buffer in `PDB_Insert`
  #define PDB_RECORD_LIMIT 32
#endif

//------------------------------------------------------------------------------------------- Types

typedef enum {
  PDB_Status_None,
  PDB_Status_Empty,
  PDB_Status_Filled,
  PDB_Status_Full
} PDB_Status_t;

typedef enum {
  PDB_Desc = 0, // newest first, reverse physical order
  PDB_Asc = 1   // oldest first, forward physical order
} PDB_Dir_t;

// Record filter, `true` keeps the record; `ctx` is `PDB_Query_t.filter_ctx`
typedef bool (*PDB_Filter_t)(const void *record, void *ctx);

//--------------------------------------------------------------------------------------- Structure

/**
 * @brief Pico database: an append-only ring of fixed-size records over flash pages.
 *   The first word of a record is its sort key, written monotonic by the caller:
 *   a timestamp, a counter, a sequence. The iterator walks the physical order, nothing
 *   is re-sorted across the page wrap. Not reentrant: one thread, or the cooperative
 *   scheduler. Torn-write recovery needs a `crc`; without one a partial record after a
 *   power loss reads as valid garbage and init may pick a half-erased page as active.
 * @param[in] page_start First flash page reserved for PDB
 * @param[in] page_count Number of flash pages (must be >= 2)
 * @param[in] payload_size User record size in bytes (>= 4, first 4B = sort key)
 * @param[in] crc CRC config or `NULL` (no integrity check, no recovery)
 * Internal:
 * @param _record_size Aligned record size (payload + CRC + pad to 8B)
 * @param _page_stop Exclusive page boundary (`page_start + page_count`)
 * @param _page_active Page currently being written
 * @param _pointer Next write address in flash
 * @param _pointer_start First record address on active page
 * @param _pointer_end Past-last record address on active page
 */
typedef struct {
  uint16_t page_start;
  uint16_t page_count;
  uint8_t payload_size;
  const CRC_t *crc;
  // internal
  uint16_t _record_size;
  uint16_t _page_stop;
  uint16_t _page_active;
  uint32_t _pointer;
  uint32_t _pointer_start;
  uint32_t _pointer_end;
} PDB_t;

/**
 * @brief Query of a selection, a zero field is no constraint.
 * @param[in] key_min Records with a key of at least `key_min`, `0` = no lower bound
 * @param[in] key_max Records with a key of at most `key_max`, `0` = no upper bound
 * @param[in] limit Records to return, `0` = every one
 * @param[in] skip Matching records to skip first, for paging
 * @param[in] dir `PDB_Desc` newest first, `PDB_Asc` oldest first
 * @param[in] filter Record filter, `NULL` = none
 * @param[in] filter_ctx Context passed to `filter`, owned by the caller
 */
typedef struct {
  uint32_t key_min;
  uint32_t key_max;
  uint32_t limit;
  uint32_t skip;
  PDB_Dir_t dir;
  PDB_Filter_t filter;
  void *filter_ctx;
} PDB_Query_t;

/**
 * @brief Iterator over a query, one record per `PDB_IterNext`.
 * @param[out] count Records returned so far
 * Internal:
 * @param _pdb Database
 * @param _query Query, copied
 * @param _page Page under the cursor
 * @param _pointer Record under the cursor
 * @param _pointer_start First slot of the page
 * @param _pointer_end Past the last slot of the page
 * @param _origin Write cursor of the database, the walk ends there
 * @param _skipped Matching records skipped so far
 * @param _steps_left Slots left to visit
 * @param _done Walk finished
 */
typedef struct {
  uint32_t count;
  // internal
  PDB_t *_pdb;
  PDB_Query_t _query;
  uint16_t _page;
  uint32_t _pointer;
  uint32_t _pointer_start;
  uint32_t _pointer_end;
  uint32_t _origin;
  uint32_t _skipped;
  uint32_t _steps_left;
  bool _done;
} PDB_Iter_t;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Scan the pages and find the write cursor, recovering a torn write or an
 *   interrupted page advance.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @return `OK` on success, `ERR` on invalid config
 */
status_t PDB_Init(PDB_t *pdb);

/**
 * @brief Append a record, with its CRC when configured. A full page moves the cursor
 *   to the next one and erases it. A failed slot is skipped and the write retried once.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @param[in] record Record of `payload_size` bytes
 * @return `OK` on success, `ERR` when the retry failed too
 */
status_t PDB_Insert(PDB_t *pdb, const void *record);

/**
 * @brief Erase every page and start over.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @return `OK` on success, `ERR` on a flash error
 */
status_t PDB_Delete(PDB_t *pdb);

/**
 * @brief Start a walk over the records matching `query`.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[out] iter Iterator, owned by the caller
 * @param[in] query Query, copied into the iterator
 * @return `OK`
 */
status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query);

/**
 * @brief Next matching record.
 * @param[in,out] iter Iterator
 * @param[out] out Room for `payload_size` bytes, `NULL` = advance only
 * @return `OK` on a record, `ERR` when the walk is over
 */
status_t PDB_IterNext(PDB_Iter_t *iter, void *out);

/**
 * @brief Record under the iterator, in flash, valid until the next insert or delete.
 * @param[in] iter Iterator after a successful `PDB_IterNext`
 * @return Pointer to the record
 */
const void *PDB_IterRef(PDB_Iter_t *iter);

/**
 * @brief Copy the records matching `query` into a buffer.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[in] query Query
 * @param[out] out Output buffer
 * @param[in] max Records that fit in `out`, `0` returns `0`
 * @return Records copied
 */
uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query, void *out, uint32_t max);

/**
 * @brief Count the records matching `query`.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[in] query Query
 * @return Matching records
 */
uint32_t PDB_Count(PDB_t *pdb, const PDB_Query_t *query);

//-------------------------------------------------------------------------------------------------
#endif
