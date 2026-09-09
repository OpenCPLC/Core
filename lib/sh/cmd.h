// lib/sh/cmd.h

#ifndef CMD_H_
#define CMD_H_

#include "log.h"
#include "stream.h"
#include "mbb.h"
#include "pwr.h"
#include "xdef.h"
#include "main.h"

//------------------------------------------------------------------------------------------ Config

#ifndef CMD_MBB_LIMIT
  // Buffers registered with `CMD_AddMemBuff`
  #define CMD_MBB_LIMIT 8
#endif

#ifndef CMD_HANDLER_LIMIT
  // Commands registered with `CMD_AddCommand`
  #define CMD_HANDLER_LIMIT 16
#endif

#ifndef CMD_BOOT
  // Shell command `boot`, the image transfer of a build under the bootloader, for tests
  #define CMD_BOOT OFF
#endif

//---------------------------------------------------------------------------------- Argument check

// Leave a handler on a wrong argument count, exact or `min, max`; `argv` and `argc` in scope
#define CMD_ArgcCount(count) \
  if(argc != (count)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_ArgcMinMax(min, max) \
  if(argc < (min) || argc > (max)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_Argc(...) _args2(__VA_ARGS__, CMD_ArgcMinMax, CMD_ArgcCount)(__VA_ARGS__)

// Leave a handler reporting argument `nbr` as invalid
#define CMD_ArgvExit(nbr) { CMD_WrongArgv(argv[0], argv[nbr], nbr); return; }

//------------------------------------------------------------------------------------------- Types

// Command handler: the tokenized line, `argv[0]` is the command
typedef void (*CMD_Handler_t)(char **argv, uint16_t argc);

// Keyword vocabulary of the shell, `hash_djb2_ci` of the lowercase word.
// Every module switches on these in its handler
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
  HASH_Boot     = 2090120089,
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
  // Update verbs
  HASH_Begin    = 254117866,
  HASH_Data     = 2090176863,
  HASH_End      = 193490716,
  HASH_Abort    = 252833149,
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
  HASH_9        = 177630
} HASH_t;

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
  RTC_Hash_Sun       = 193506203
} RTC_Hash_t;

typedef enum {
  PWR_Hash_Stop        = 2090736459,
  PWR_Hash_Stop0       = 274826459,
  PWR_Hash_Stop1       = 274826460,
  PWR_Hash_StandbySram = 950227578,
  PWR_Hash_Standbysram = 1332813965,
  PWR_Hash_Standby     = 2916655642,
  PWR_Hash_Shutdown    = 4232446817
} PWR_Hash_t;

//----------------------------------------------------------------------------------------- Globals

// Active buffer: target of the `mbb` verbs and of handlers producing bulk output.
// The first registered buffer, then whatever `mbb select <name>` picked
extern MBB_t *CmdMbb;

//--------------------------------------------------------------------------------------------- API

/**
 * @brief Register a buffer with the shell. Its `name` must be set, the hash of it is
 *   cached for the lookup. The flash mirror is loaded when the buffer has one.
 * @param[in] mbb Buffer with `name` set
 */
void CMD_AddMemBuff(MBB_t *mbb);

/**
 * @brief Register a command. A repeated name replaces the handler with a warning,
 *   `NULL` registers the default handler called when no command matches.
 * @param[in] name Command word, matched case-insensitively
 * @param[in] handler Handler
 */
void CMD_AddCommand(const char *name, CMD_Handler_t handler);

// Save the active buffer to its flash mirror after every modifying command
void CMD_SetAutosave(bool autosave);

// Hooks of the `pwr` verbs, `NULL` restores `PWR_Sleep` and the deferred `DbgReset`
void CMD_SetSleep(void (*Sleep)(PWR_SleepMode_t));
void CMD_SetReset(void (*Reset)(void));

// Hook called for every line the console receives, before it is handled
void CMD_SetActivity(void (*Activity)(void));

// Error lines behind `CMD_Argc` and `CMD_ArgvExit`
void CMD_WrongArgc(const char *cmd, uint16_t argc);
void CMD_WrongArgv(const char *cmd, const char *argv, uint16_t pos);

/**
 * @brief Take one line from the stream and dispatch it.
 * @param[in,out] stream Input stream: console, USB or any `STREAM_t`
 * @return `true` when a line was handled, `false` when nothing waited
 */
bool CMD_Step(STREAM_t *stream);

//---------------------------------------------------------------------------------------- Triggers

// Code set by the `trig` command, taken once; `1` when no code was given
uint16_t TRIG_Event(void);

// Wait for any trigger or for `code`, yielding to the scheduler
uint16_t TRIG_Wait(void);
void TRIG_WaitFor(uint16_t code);

//-------------------------------------------------------------------------------------------------
#endif
