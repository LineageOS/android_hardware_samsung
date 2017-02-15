/*
 * Copyright (C) 2017 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SAMSUNG_MACLOADER_H
#define SAMSUNG_MACLOADER_H

/*
 * Board specific nodes
 *
 * If your kernel exposes these controls in another place, you can either
 * symlink to the locations given here, or override this header in your
 * device tree.
 */

/* NVRAM calibration, NULL if calibration unneeded */
#define WIFI_DRIVER_NVRAM_PATH "/system/etc/wifi/nvram_net.txt"

/* NVRAM calibration parameters */
#define WIFI_DRIVER_NVRAM_PATH_PARAM "/sys/module/dhd/parameters/nvram_path"

/* Physical address (MAC) */
#define MACADDR_PATH "/efs/wifi/.mac.info"

/* Consumer identification number (CID) */
#define CID_PATH "/data/.cid.info"

#endif // SAMSUNG_MACLOADER_H
