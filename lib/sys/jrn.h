// lib/sys/jrn.h

#ifndef JRN_H_
#define JRN_H_

#include <stdint.h>
#include <stdbool.h>
#include "mbb.h"
#include "pdb.h"
#include "log.h"
#include "main.h"

#ifndef JRN_LOG
  // Log function for `Journal` messages
  #define JRN_LOG(fmt, ...) LOG_LIB_DBG("jrn", fmt, ##__VA_ARGS__)
#endif

//--------------------------------------------------------------------------------------- Types

/**
 * @brief Journal record stored in PDB.
 * `time` is the sort key (first 4B: required by PDB contract). Captured
 * automatically by `JRN_Insert` from RTC, or from `tick_keep` when RTC
 * is not initialized.
 * @param time Unix timestamp from RTC, or boot tick as fallback
 * @param code User-defined event code
 */
typedef struct {
  uint32_t time;
  uint16_t code;
} JRN_t;

//----------------------------------------------------------------------------------------- API

/**
 * @brief Initialize journal database.
 * Single-instance module: stores `pdb` pointer internally for later calls.
 * `payload_size` is set automatically to `sizeof(JRN_t)`.
 * Safe under cooperative schedulers (VRTS): all operations complete without
 * yielding. Not safe under preemptive RTOS or from ISR: wrap externally.
 * @param[in,out] pdb Pre-configured `PDB_t` instance (`page_start`, `page_count`, `crc`)
 * @return `OK` on success, `ERR` on `NULL` pointer or flash error
 */
status_t JRN_Init(PDB_t *pdb);

/**
 * @brief Insert journal record. Time is captured automatically from RTC,
 * or from `tick_keep` if RTC is not initialized.
 * @param[in] code Event code
 * @return `OK` on success, `ERR` on flash error
 */
status_t JRN_Insert(uint16_t code);

/**
 * @brief Erase all journal records.
 * @return `OK` on success, `ERR` on flash error
 */
status_t JRN_Delete(void);

/**
 * @brief Select journal records matching query into MBB buffer.
 * Caller `query.filter` is honored (not overridden).
 * `mbb->size` is always reset on entry.
 * @param[in] query Query parameters
 * @param[out] mbb Output buffer (filled with `JRN_t` records)
 * @return Number of records
 */
uint32_t JRN_Select(const PDB_Query_t *query, MBB_t *mbb);

/**
 * @brief Select journal records filtered by code.
 * Reentrant: filter context lives on caller stack.
 * `mbb->size` is always reset on entry.
 * @param[in] query Query parameters (caller `filter`/`filter_ctx` are overridden)
 * @param[in] code Event code to match
 * @param[out] mbb Output buffer
 * @return Number of records
 */
uint32_t JRN_SelectCode(const PDB_Query_t *query, uint16_t code, MBB_t *mbb);

/**
 * @brief Count journal records matching query without copying.
 * @param[in] query Query parameters
 * @return Number of matching records
 */
uint32_t JRN_Count(const PDB_Query_t *query);

/**
 * @brief Compact buffer: extract only timestamps from journal records.
 * Overwrites `JRN_t[]` with `uint32_t[]` timestamps in place.
 * Safe: `JRN_t.time` is at offset 0, written before later reads.
 * @param[in,out] mbb Buffer with `JRN_t` records
 */
void JRN_GetTimestamp(MBB_t *mbb);

//---------------------------------------------------------------------------------------------
#endif