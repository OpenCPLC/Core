/** @file lib/sh/cmd.h */

#ifndef CMD_H_
#define CMD_H_

#include "log.h"
#include "stream.h"
#include "pwr.h"
#include "main.h"

//-------------------------------------------------------------------------------------------------

#ifndef CMD_MBB_LIMIT
  #define CMD_MBB_LIMIT 8
#endif

#ifndef CMD_CALLBACK_LIMIT
  #define CMD_CALLBACK_LIMIT 16
#endif

#define CMD_ArgcCount(count) if(argc != (count)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_ArgcMinMax(min, max) if(argc < (min) || argc > (max)) { CMD_WrongArgc(argv[0], argc); return; }
#define CMD_ArgcGet(_1, _2, NAME, ...) NAME
#define CMD_Argc(...) CMD_ArgcGet(__VA_ARGS__, CMD_ArgcMinMax, CMD_ArgcCount)(__VA_ARGS__)
#define CMD_ArgvExit(nbr) { CMD_WrongArgv(argv[0], argv[nbr], nbr); return; }

//-------------------------------------------------------------------------------------------------

typedef enum {
  HASH_Ping = 2090616627,
  HASH_Trig = 2090770011,
  HASH_File = 2090257189,
  HASH_Uid = 193507975,
  HASH_Rtc = 193505070,
  HASH_Alarm = 253177266,
  HASH_Pwr = 193503006,
  HASH_Power = 271097426,
  HASH_Addr = 2090071808,
  HASH_List = 2090473057,
  HASH_Active = 4049882593,
  HASH_Select = 461431749,
  HASH_Info = 2090370257,
  HASH_Clear = 255552908,
  HASH_Load = 2090478981,
  HASH_Save = 2090715988,
  HASH_Append = 4065151197,
  HASH_Flash = 259106899,
  HASH_Mutex = 267752024,
  HASH_Print = 271190290,
  HASH_Set = 193505681,
  HASH_Rst = 193505054,
  HASH_Reset = 273105544,
  HASH_Copy = 2090156064,
  HASH_To = 5863848,
  HASH_From = 2090267097,
  HASH_A = 177670,
  HASH_B = 177671,
  HASH_Sleep = 274527774,
  HASH_Reboot = 421948272,
  HASH_Restart = 1059716234,
  HASH_Now = 193500569,
  HASH_On = 5863682,
  HASH_Start = 274811347,
  HASH_Enable = 4218778540,
  HASH_Off = 193501344,
  HASH_Stop = 2090736459,
  HASH_Disable = 314893497,
  HASH_Tgl = 193506828,
  HASH_Toggle = 512249127,
  HASH_Sw = 5863823,
  HASH_Switch = 482686839,
  HASH_Pulse = 271301518,
  HASH_Impulse = 2630979716,
  HASH_Burst = 254705173,
  HASH_Duty = 2090198667,
  HASH_Fill = 2090257196,
  HASH_0 = 177621,
  HASH_1 = 177622,
  HASH_2 = 177623,
  HASH_3 = 177624,
  HASH_4 = 177625,
  HASH_5 = 177626,
  HASH_6 = 177627,
  HASH_7 = 177628,
  HASH_8 = 177629,
  HASH_9 = 177630
} HASH_t;

#ifdef RTC_H_
typedef enum {
  RTC_Hash_Everyday = 552618222,
  RTC_Hash_Monday = 238549325,
  RTC_Hash_Tuesday = 4252182340,
  RTC_Hash_Wednesday = 1739173961,
  RTC_Hash_Thursday = 3899371353,
  RTC_Hash_Friday = 4262946948,
  RTC_Hash_Saturday = 3744646578,
  RTC_Hash_Sunday = 480477209,
  RTC_Hash_Evd = 193490980,
  RTC_Hash_Mon = 193499471,
  RTC_Hash_Tue = 193507283,
  RTC_Hash_Wed = 193510021,
  RTC_Hash_Thu = 193506870,
  RTC_Hash_Fri = 193491942,
  RTC_Hash_Sat = 193505549,
  RTC_Hash_Sun = 193506203
} RTC_Hash_t;
#endif

typedef enum {
  PWR_Hash_Stop = 2090736459,
  PWR_Hash_Stop0 = 274826459,
  PWR_Hash_Stop1 = 274826460,
  PWR_Hash_StandbySram = 950227578,
  PWR_Hash_Standbysram = 1332813965,
  PWR_Hash_Standby = 2916655642,
  PWR_Hash_Shutdown = 4232446817,
} PWR_Hash_t;

//-------------------------------------------------------------------------------------------------

/**
 * @brief Register file with command processor.
 * @param[in] file File to register
 */
void CMD_AddFile(MBB_t *file);

/**
 * @brief Register command callback.
 * @param[in] callback Command handler function
 * @param[in] argv0 Command name (NULL for default handler)
 */
void CMD_AddCallback(void (*callback)(char **, uint16_t), char *argv0);

/**
 * @brief Enable/disable automatic flash save after file operations.
 * @param[in] autosave Enable flag
 */
void CMD_FlashAutosave(bool autosave);

/**
 * @brief Set custom sleep handler for `pwr sleep` command.
 * @param[in] Sleep Sleep function (NULL to use default PWR_Sleep)
 */
void CMD_SetSleep(void (*Sleep)(PWR_SleepMode_t));

/**
 * @brief Set custom reset handler for `pwr reset` command.
 * @param[in] Reset Reset function (NULL to use default DbgReset flag)
 */
void CMD_SetReset(void (*Reset)(void));

/**
 * @brief Report wrong argument count error.
 * @param[in] cmd Command name
 * @param[in] argc Actual argument count
 */
void CMD_WrongArgc(char *cmd, uint16_t argc);

/**
 * @brief Report invalid argument error.
 * @param[in] cmd Command name
 * @param[in] argv Argument value
 * @param[in] pos Argument position
 */
void CMD_WrongArgv(char *cmd, char *argv, uint16_t pos);

/**
 * @brief Process commands from stream.
 * @param[in,out] stream Input stream
 * @return `true` if command was processed
 */
bool CMD_Loop(STREAM_t *stream);

//-------------------------------------------------------------------------------------------------

/**
 * @brief Get and clear trigger event.
 * @return Trigger code or `0` if none
 */
uint16_t TRIG_Event(void);

/**
 * @brief Wait for any trigger (cooperative).
 * @return Trigger code
 */
uint16_t TRIG_Wait(void);

/**
 * @brief Wait for specific trigger code (cooperative).
 * @param[in] code Expected trigger code
 */
void TRIG_WaitFor(uint16_t code);

//-------------------------------------------------------------------------------------------------

#endif