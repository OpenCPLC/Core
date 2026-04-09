// lib/sh/cmd.h

#ifndef CMD_H_
#define CMD_H_

#include "log.h"
#include "stream.h"
#include "mbb.h"
#include "pwr.h"
#include "xdef.h"
#include "main.h"

//-------------------------------------------------------------------------------- Configurable

#ifndef CMD_MBB_LIMIT
  // Max number of `MBB_t` buffers registered via `CMD_AddMemBuff`
  #define CMD_MBB_LIMIT 8
#endif

#ifndef CMD_HANDLER_LIMIT
  // Max number of command handlers registered via `CMD_AddCommand`
  #define CMD_HANDLER_LIMIT 16
#endif

//----------------------------------------------------------------------------- Argv validation

#define CMD_ArgcCount(count) \
  if(argc != (count)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_ArgcMinMax(min, max) \
  if(argc < (min) || argc > (max)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_Argc(...) _args2(__VA_ARGS__, CMD_ArgcMinMax, CMD_ArgcCount)(__VA_ARGS__)
#define CMD_ArgvExit(nbr) { CMD_WrongArgv(argv[0], argv[nbr], nbr); return; }

//--------------------------------------------------------------------------------------- Types

// Command handler signature: receives `argv` and `argc` from parsed input line
typedef void (*CMD_Handler_t)(char **argv, uint16_t argc);

//---------------------------------------------------------------------------------------- Hash

/**
 * Shared keyword vocabulary for the entire CMD ecosystem.
 * All hashes computed from lowercase strings (parser uses `hash_djb2_ci`).
 * Each section groups related words; modules switch on these in their handlers.
 */
typedef enum {
  // Top-level commands
  HASH_Ping     = 2090616627,
  HASH_Trig     = 2090770011,
  HASH_Mbb      = 193499030,
  HASH_Uid      = 193507975,
  HASH_Rtc      = 193505070,
  HASH_Alarm    = 253177266,
  HASH_Pwr      = 193503006,
  HASH_Power    = 271097426,
  HASH_Addr     = 2090071808,
  HASH_Flash    = 259106899,
  HASH_Mutex    = 267752024,
  // MBB verbs
  HASH_Save     = 2090715988,
  HASH_Load     = 2090478981,
  HASH_Append   = 4065151197,
  HASH_Clear    = 255552908,
  HASH_Print    = 271190290,
  HASH_Copy     = 2090156064,
  HASH_From     = 2090267097,
  HASH_To       = 5863848,
  HASH_List     = 2090473057,
  HASH_Info     = 2090370257,
  HASH_Select   = 461431749,
  HASH_Active   = 4049882593,
  // Database verbs
  HASH_Insert   = 81003162,
  HASH_Delete   = 4169368696,
  HASH_Asc      = 193486524,
  HASH_Desc     = 2090181188,
  HASH_Count    = 255678574,
  // Power verbs
  HASH_Sleep    = 274527774,
  HASH_Reboot   = 421948272,
  HASH_Restart  = 1059716234,
  HASH_Reset    = 273105544,
  HASH_Rst      = 193505054,
  // State control
  HASH_Set      = 193505681,
  HASH_On       = 5863682,
  HASH_Off      = 193501344,
  HASH_Start    = 274811347,
  HASH_Stop     = 2090736459,
  HASH_Enable   = 4218778540,
  HASH_Disable  = 314893497,
  HASH_Tgl      = 193506828,
  HASH_Toggle   = 512249127,
  HASH_Sw       = 5863823,
  HASH_Switch   = 482686839,
  // Signal generation
  HASH_Pulse    = 271301518,
  HASH_Impulse  = 2630979716,
  HASH_Burst    = 254705173,
  HASH_Duty     = 2090198667,
  HASH_Fill     = 2090257196,
  // Time
  HASH_Now      = 193500569,
  // Slot literals
  HASH_A        = 177670,
  HASH_B        = 177671,
  HASH_0        = 177621,
  HASH_1        = 177622,
  HASH_2        = 177623,
  HASH_3        = 177624,
  HASH_4        = 177625,
  HASH_5        = 177626,
  HASH_6        = 177627,
  HASH_7        = 177628,
  HASH_8        = 177629,
  HASH_9        = 177630,
} HASH_t;

#ifdef RTC_H_
typedef enum {
  RTC_Hash_Everyday  = 552618222,
  RTC_Hash_Monday    = 238549325,
  RTC_Hash_Tuesday   = 4252182340,
  RTC_Hash_Wednesday = 1739173961,
  RTC_Hash_Thursday  = 3899371353,
  RTC_Hash_Friday    = 4262946948,
  RTC_Hash_Saturday  = 3744646578,
  RTC_Hash_Sunday    = 480477209,
  RTC_Hash_Evd       = 193490980,
  RTC_Hash_Mon       = 193499471,
  RTC_Hash_Tue       = 193507283,
  RTC_Hash_Wed       = 193510021,
  RTC_Hash_Thu       = 193506870,
  RTC_Hash_Fri       = 193491942,
  RTC_Hash_Sat       = 193505549,
  RTC_Hash_Sun       = 193506203,
} RTC_Hash_t;
#endif

typedef enum {
  PWR_Hash_Stop        = 2090736459,
  PWR_Hash_Stop0       = 274826459,
  PWR_Hash_Stop1       = 274826460,
  PWR_Hash_StandbySram = 950227578,
  PWR_Hash_Standbysram = 1332813965,
  PWR_Hash_Standby     = 2916655642,
  PWR_Hash_Shutdown    = 4232446817,
} PWR_Hash_t;

//------------------------------------------------------------------------------------- Globals

/**
 * @brief Currently active `MBB_t` buffer.
 * Default target for command handlers that produce output (e.g. `event select`).
 * Set to first registered MBB on startup, changed via `mbb select <name>` command.
 */
extern MBB_t *cmd_mbb;

//----------------------------------------------------------------------------------------- API

/**
 * @brief Register `MBB_t` buffer with command processor.
 * Buffer name (`mbb->name`) must be set before calling.
 * Hash of name is cached for fast lookup. If flash autosave is enabled,
 * buffer content is loaded from flash automatically.
 * First registered buffer becomes `cmd_mbb` (active) by default.
 * @param[in] mbb Pointer to `MBB_t` instance with `name` set
 */
void CMD_AddMemBuff(MBB_t *mbb);

/**
 * @brief Register command handler.
 * If a handler with the same `name` already exists, it is overwritten and
 * a warning is logged. Pass `NULL` as `name` to register the default handler
 * (called when no other command matches).
 * @param[in] name Command name (lowercase, or `NULL` for default handler)
 * @param[in] handler Function called when command is matched
 */
void CMD_AddCommand(const char *name, CMD_Handler_t handler);

/**
 * @brief Enable/disable automatic flash save after MBB content changes.
 * @param[in] autosave `true` = save MBB to flash on every modifying command
 */
void CMD_SetAutosave(bool autosave);

/**
 * @brief Set custom sleep handler for `pwr sleep` command.
 * @param[in] Sleep Sleep function or `NULL` to use default `PWR_Sleep`
 */
void CMD_SetSleep(void (*Sleep)(PWR_SleepMode_t));

/**
 * @brief Set custom reset handler for `pwr reset` command.
 * @param[in] Reset Reset function or `NULL` to use default debug-flag reset
 */
void CMD_SetReset(void (*Reset)(void));

/**
 * @brief Report wrong argument count error.
 * Internal — called by `CMD_Argc*` macros, not meant for direct use.
 * @param[in] cmd Command name (`argv[0]`)
 * @param[in] argc Actual argument count
 */
void CMD_WrongArgc(char *cmd, uint16_t argc);

/**
 * @brief Report invalid argument error.
 * Internal — called by `CMD_ArgvExit` macro, not meant for direct use.
 * @param[in] cmd Command name (`argv[0]`)
 * @param[in] argv Argument value
 * @param[in] pos Argument position (0-indexed)
 */
void CMD_WrongArgv(char *cmd, char *argv, uint16_t pos);

/**
 * @brief Process one command from input stream.
 * Reads available data, parses on newline, dispatches to matching handler.
 * Call this in main loop or dedicated CMD task.
 * @param[in,out] stream Input stream (UART, USB CDC, etc.)
 * @return `true` if a command was processed, `false` if no input ready
 */
bool CMD_Step(STREAM_t *stream);

//------------------------------------------------------------------------------------ Triggers

/**
 * @brief Get and clear pending trigger event.
 * @return Trigger code or `0` if no trigger pending
 */
uint16_t TRIG_Event(void);

/**
 * @brief Wait for any trigger (cooperative — yields to scheduler).
 * @return Trigger code received
 */
uint16_t TRIG_Wait(void);

/**
 * @brief Wait for specific trigger code (cooperative — yields to scheduler).
 * @param[in] code Expected trigger code
 */
void TRIG_WaitFor(uint16_t code);

//---------------------------------------------------------------------------------------------
#endif