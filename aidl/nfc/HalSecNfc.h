/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/nfc/INfc.h>
#include <hardware/nfc.h>

int nfc_hal_init(void);
void nfc_hal_deinit(void);
int nfc_hal_open(nfc_stack_callback_t* p_cback,
                 nfc_stack_data_callback_t* p_data_cback);
int nfc_hal_write(uint16_t data_len, const uint8_t* p_data);
int nfc_hal_core_initialized_for_aidl();
int nfc_hal_pre_discover();
int nfc_hal_close();
int nfc_hal_control_granted();
int nfc_hal_power_cycle();

int nfc_hal_factory_reset(void);
int nfc_hal_closeForPowerOffCase(void);

void nfc_hal_setLogging(bool enable);
bool nfc_hal_isLoggingEnabled();

