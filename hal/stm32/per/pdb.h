// hal/stm32/pdb.h

#ifndef PDB_H_
#define PDB_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "flash.h"
#include "xdef.h"
#include "crc.h"
#include "log.h"
#include "main.h"

#ifndef PDB_LOG
  // Log function for `PDB` messages
  #define PDB_LOG(fmt, ...) LOG_LIB_DBG("pdb", fmt, ##__VA_ARGS__)
#endif

#ifndef PDB_RECORD_LIMIT
  // Max record size: payload + CRC + 8B align padding. Stack buffer in `PDB_Insert`.
  #define PDB_RECORD_LIMIT 32
#endif

//--------------------------------------------------------------------------------------- Types

typedef enum {
  PDB_Status_None,
  PDB_Status_Empty,
  PDB_Status_Filled,
  PDB_Status_Full,
} PDB_Status_t;

typedef enum {
  PDB_Desc = 0, // Newest first (reverse physical order)
  PDB_Asc = 1   // Oldest first (forward physical order)
} PDB_Dir_t;

// Filter callback: return `true` to include record. `ctx` from `PDB_Query_t.filter_ctx`.
typedef bool (*PDB_Filter_t)(const void *record, void *ctx);

/**
 * @brief PDB (picoDatabase) instance.
 * Append-only circular flash log with fixed-size records.
 * First `uint32_t` of each record is a monotonic sort key
 * (timestamp, counter, sequence: user decides). Iterator `dir`
 * traverses physical layout — assumes user writes monotonic keys,
 * order across page wrap-around is not re-sorted.
 * Requires STM32 doubleword (8B) flash write via `FLASH_Write`.
 * Not reentrant — single-threaded or cooperative scheduler (VRTS) only.
 * Torn-write recovery requires `crc != NULL`. Without CRC, partially
 * written records after power loss may be read as valid garbage.
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
  uint8_t page_start;
  uint8_t page_count;
  uint8_t payload_size;
  const CRC_t *crc;
  // internal
  uint16_t _record_size;
  uint8_t _page_stop;
  uint8_t _page_active;
  uint32_t _pointer;
  uint32_t _pointer_start;
  uint32_t _pointer_end;
} PDB_t;

/**
 * @brief Query parameters for iteration/select.
 * Zero-initialized fields mean "no constraint".
 * @param[in] key_min Include records with key >= `key_min` (0 = no lower bound)
 * @param[in] key_max Include records with key <= `key_max` (0 = no upper bound)
 * @param[in] limit Maximum records to return (0 = unlimited)
 * @param[in] skip Number of matching records to skip (pagination)
 * @param[in] dir `PDB_Desc` newest first, `PDB_Asc` oldest first
 * @param[in] filter Callback or `NULL` (no filter)
 * @param[in] filter_ctx User context passed to `filter` (lifetime: caller's responsibility)
 */
typedef struct {
  uint32_t key_min;
  uint32_t key_max;
  uint16_t limit;
  uint16_t skip;
  PDB_Dir_t dir;
  PDB_Filter_t filter;
  void *filter_ctx;
} PDB_Query_t;

/**
 * @brief Iterator state for record-by-record traversal.
 * @param count Records returned so far
 */
typedef struct {
  uint16_t count;
  // internal
  PDB_t *_pdb;
  PDB_Query_t _query;
  uint8_t _page;
  uint32_t _pointer;
  uint32_t _pointer_start;
  uint32_t _pointer_end;
  uint32_t _origin;
  uint16_t _skipped;
  bool _done;
} PDB_Iter_t;

//----------------------------------------------------------------------------------------- API

/**
 * @brief Initialize PDB instance.
 * Scans flash pages, recovers from torn writes (CRC required) and post-wrap state.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @return `OK` on success, `ERR` on invalid config or flash error
 */
status_t PDB_Init(PDB_t *pdb);

/**
 * @brief Append record to database.
 * Writes payload (+ CRC if configured) to flash.
 * Advances to next page and erases it when current page is full.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @param[in] record Pointer to user record (`payload_size` bytes)
 * @return `OK` on success, `ERR` on flash error
 */
status_t PDB_Insert(PDB_t *pdb, const void *record);

/**
 * @brief Erase all pages and reinitialize.
 * @param[in,out] pdb Pointer to `PDB_t` instance
 * @return `OK` on success, `ERR` on flash error
 */
status_t PDB_Delete(PDB_t *pdb);

/**
 * @brief Initialize iterator for record traversal.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[out] iter Iterator state (caller-allocated)
 * @param[in] query Query parameters (copied into iterator)
 * @return `OK` always
 */
status_t PDB_IterInit(PDB_t *pdb, PDB_Iter_t *iter, const PDB_Query_t *query);

/**
 * @brief Fetch next matching record.
 * Copies `payload_size` bytes to `out`.
 * Pass `NULL` to advance without copying (for counting).
 * @param[in,out] iter Iterator state
 * @param[out] out Buffer for record or `NULL`
 * @return `OK` if record found, `ERR` if no more records
 */
status_t PDB_IterNext(PDB_Iter_t *iter, void *out);

/**
 * @brief Zero-copy reference to current iterator record.
 * Returns pointer directly into memory-mapped flash.
 * Valid until next `PDB_Insert` or `PDB_Delete`.
 * @param[in] iter Iterator (after successful `PDB_IterNext`)
 * @return Pointer to record in flash
 */
const void *PDB_IterRef(PDB_Iter_t *iter);

/**
 * @brief Bulk select: copy matching records into buffer.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[in] query Query parameters
 * @param[out] out Output buffer
 * @param[in] max Maximum records that fit in `out` (0 = no buffer, returns 0)
 * @return Number of records copied
 */
uint32_t PDB_Select(PDB_t *pdb, const PDB_Query_t *query, void *out, uint32_t max);

/**
 * @brief Count matching records without copying.
 * @param[in] pdb Pointer to `PDB_t` instance
 * @param[in] query Query parameters
 * @return Number of matching records
 */
uint32_t PDB_Count(PDB_t *pdb, const PDB_Query_t *query);

//---------------------------------------------------------------------------------------------
#endif