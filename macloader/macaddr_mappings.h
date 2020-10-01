/*
 * Copyright (C) 2017-2018, The LineageOS Project
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

#ifndef MACADDR_MAPPINGS_H
#define MACADDR_MAPPINGS_H

#define MAX_RANGE_ENTRIES 100
#define RANGE_ENTRY_LEN 9

enum Type {
    NONE,
    MURATA,
    SEMCOSH,
    SEMCO3RD,
    SEMCO,
    WISOL,
    TYPE_MAX = WISOL
};

struct company_range {
    int type;
    char macaddrs[MAX_RANGE_ENTRIES][RANGE_ENTRY_LEN];
};

/*
 * address mappings from http://hwaddress.com
 */

static const struct company_range murata_ranges = {
    .type = MURATA,
    .macaddrs = {
        "00:0e:6d",
        "00:13:e0",
        "00:21:e8",
        "00:26:e8",
        "00:37:6d",
        "00:60:57",
        "00:9d:6b",
        "00:ae:fa",
        "04:46:65",
        "10:5f:06",
        "10:98:c3",
        "10:a5:d0",
        "10:d5:42",
        "14:7d:c5",
        "1c:70:22",
        "1c:99:4c",
        "20:02:af",
        "40:f3:08",
        "44:91:60",
        "44:a7:cf",
        "5c:da:d4",
        "5c:f8:a1",
        "60:21:c0",
        "60:f1:89",
        "78:4b:87",
        "78:52:1a",
        "88:30:8a",
        "8c:45:00",
        "90:b6:86",
        "98:f1:70",
        "a0:c9:a0",
        "a0:cc:2b",
        "a4:08:ea",
        "b0:72:bf",
        "b8:d7:af",
        "c8:14:79",
        "cc:c0:79",
        "d0:e4:4a",
        "d8:c4:6a",
        "dc:ef:ca",
        "f0:27:65",
        "fc:c2:de",
        "fc:db:b3"
    }
};

static const struct company_range semcosh_ranges = {
    .type = SEMCOSH,
    .macaddrs = {
        "34:23:ba",
        "38:aa:3c",
        "5c:0a:5b",
        "6c:c7:ec",
        "88:32:9b",
        "90:18:7c",
        "cc:3a:61"
    }
};

static const struct company_range semco3rd_ranges = {
    .type = SEMCO3RD,
    .macaddrs = {
        "04:d6:aa",
        "08:c5:e1",
        "24:18:1d",
        "2c:0e:3d",
        "30:07:4d",
        "54:88:0e",
        "84:38:38",
        "8c:f5:a3",
        "ac:36:13",
        "ac:5f:3e",
        "b4:79:a7",
        "c0:97:27",
        "c0:bd:d1",
        "c8:ba:94",
        "d0:22:be",
        "d0:25:44",
        "e8:50:8b",
        "ec:1f:72",
        "ec:9b:f3",
        "f4:09:d8",
        "f8:04:2e"
    }
};

static const struct company_range semco_ranges = {
    .type = SEMCO,
    .macaddrs = {
        "48:13:7e",
        "4c:66:41",
        "51:f6:6b",
        "94:76:b7",
        "a4:84:31",
        "cc:07:ab",
        "d8:c4:e9",
        "e8:3a:12",
        "ec:9b:f3",
        "f0:25:b7",
        "f8:e6:1a"
    }
};

static const struct company_range wisol_ranges = {
    .type = WISOL,
    .macaddrs = {
        "48:5a:3f",
        "70:2c:1f"
    }
};

static const struct company_range *all_ranges[TYPE_MAX] = {
    &murata_ranges,
    &semcosh_ranges,
    &semco3rd_ranges,
    &semco_ranges,
    &wisol_ranges
};

#endif // MACADDR_MAPPINGS_H
