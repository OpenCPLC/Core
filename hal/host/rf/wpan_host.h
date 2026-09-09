// hal/host/rf/wpan_host.h

// Test hooks of the host CPU2 mailbox model; the modelled API itself is `wpan_wb.h`.
// A test scripts responses ahead of the commands and injects events like the radio would.

#ifndef WPAN_HOST_H_
#define WPAN_HOST_H_

#include <stdbool.h>
#include <stdint.h>

//--------------------------------------------------------------------------------------------- API

// Forget everything: captured commands, scripted responses, pending events
void WPAN_HostReset(void);

/**
 * @brief Script the response of an upcoming command, consumed in call order.
 *   A command finding the queue empty answers status `0` with no parameters.
 * @param[in] status Status byte of the response
 * @param[in] rsp Return parameters after the status byte, may be `NULL`
 * @param[in] len Length of `rsp` [B]
 */
void WPAN_HostRespond(uint8_t status, const uint8_t *rsp, uint8_t len);

/**
 * @brief Take the oldest captured command.
 * @param[out] param Parameter bytes, room for `255`, may be `NULL`
 * @param[out] len Parameter length [B], may be `NULL`
 * @return Opcode, `0` when nothing was captured
 */
uint16_t WPAN_HostCommand(uint8_t *param, uint8_t *len);

/**
 * @brief Inject an asynchronous event the way the CPU2 posts one.
 * @param[in] event Event bytes: packet type, event code, payload length, payload
 * @param[in] len Length of `event` [B]
 */
void WPAN_HostEvent(const uint8_t *event, uint8_t len);

// Stack sizing captured from `WPAN_BleStackInit`
uint16_t WPAN_HostAttributes(void);
uint16_t WPAN_HostValues(void);

//-------------------------------------------------------------------------------------------------
#endif
