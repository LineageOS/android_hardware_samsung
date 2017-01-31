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

#ifndef SAMSUNG_AUDIO_H
#define SAMSUNG_AUDIO_H

/*
 * Sound card specific defines.
 *
 * This is an example configuration for a WolfsonMicro WM1814 sound card.
 * Codec: Vegas
 *
 * If you driver does not support one of the devices, the id should not be
 * defined.
 */

#define MIXER_CARD 0
#define SOUND_CARD 0

/* Playback */
#define SOUND_DEEP_BUFFER_DEVICE 3
#define SOUND_PLAYBACK_DEVICE 4
#define SOUND_PLAYBACK_SCO_DEVICE 2

/* Capture */
#define SOUND_CAPTURE_DEVICE 0
#define SOUND_CAPTURE_SCO_DEVICE 2

/* Unusupported
#define SOUND_CAPTURE_LOOPBACK_AEC_DEVICE 1
#define SOUND_CAPTURE_HOTWORD_DEVICE 0
*/

/*
 * If the device has stereo speakers and the speakers are arranged on
 * different sides of the device you can activate this feature by
 * setting it to 1.
 */
#define SWAP_SPEAKER_ON_SCREEN_ROTATION 0

/*
 * You can that this to 1 if your kernel supports irq affinity for
 * fast mode. See /proc/asound/irq_affinity
 */
#define SUPPORTS_IRQ_AFFINITY 0

#endif // SAMSUNG_AUDIO_H
