/*
 *    Copyright (C) 2013 SAMSUNG S.LSI
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include "product.h"

eNFC_SNFC_PRODUCTS product_map(uint8_t* ver, uint16_t* base_address) {
  size_t i;
  for (i = 0; i < sizeof(_n3_bl_ver_list) / 6; i++) {
    if (ver[0] == _n3_bl_ver_list[i][0] && ver[1] == _n3_bl_ver_list[i][1] &&
        ver[2] == _n3_bl_ver_list[i][2] && ver[3] == _n3_bl_ver_list[i][3]) {
      *base_address = _n3_bl_ver_list[i][5] * 0x100;
      return (eNFC_SNFC_PRODUCTS)_n3_bl_ver_list[i][4];
    }
  }

  for (i = 0; i < sizeof(_n5_bl_ver_list) / 6; i++) {
    if (ver[0] == _n5_bl_ver_list[i][0] && ver[1] == _n5_bl_ver_list[i][1]
        //&& ver[2] == _n5_bl_ver_list[i][2]  // Customer key
        && ver[3] == _n5_bl_ver_list[i][3]) {
      *base_address = _n5_bl_ver_list[i][5] * 0x100;
            return  (eNFC_SNFC_PRODUCTS) _n5_bl_ver_list[i][4];
    }
  }

  for (i = 0; i < sizeof(_n7_bl_ver_list) / 6; i++) {
    if (ver[0] == _n7_bl_ver_list[i][0] && ver[1] == _n7_bl_ver_list[i][1]
        //&& ver[2] == _n7_bl_ver_list[i][2]  // Customer key
        && ver[3] == _n7_bl_ver_list[i][3]) {
      *base_address = _n7_bl_ver_list[i][5] * 0x100;
            return (eNFC_SNFC_PRODUCTS) _n7_bl_ver_list[i][4];
    }
  }

  for (i = 0; i < sizeof(_n8_bl_ver_list) / 6; i++) {
    if (ver[0] == _n8_bl_ver_list[i][0] && ver[1] == _n8_bl_ver_list[i][1]
        //&& ver[2] == _n7_bl_ver_list[i][2]  // Customer key
        && ver[3] == _n8_bl_ver_list[i][3]) {
      *base_address = _n8_bl_ver_list[i][5] * 0x100;
            return (eNFC_SNFC_PRODUCTS) _n8_bl_ver_list[i][4];
    }
  }

  for (i = sizeof(_n8_bl_ver_list) / 6 -1 ; i  > 0 ;  i--) {
    if (ver[0] == _n8_bl_ver_list[i][0] && ver[1] == _n8_bl_ver_list[i][1]
        && ver[3] >= _n8_bl_ver_list[i][3]) {
      *base_address = _n8_bl_ver_list[i][5] * 0x100;
      return (eNFC_SNFC_PRODUCTS) _n8_bl_ver_list[i][4];
    }
  }

  for (i = 0; i < sizeof(_n9_bl_ver_list) / 6; i++) {
    if (ver[0] == _n9_bl_ver_list[i][0]
        && ver[1] == _n9_bl_ver_list[i][1]
        //&& ver[2] == _n9_bl_ver_list[i][2]  // Customer key
        && ver[3] == _n9_bl_ver_list[i][3]) {
        return (eNFC_SNFC_PRODUCTS) _n9_bl_ver_list[i][4];
    }
  }

  return SNFC_UNKNOWN;
}

const char* get_product_name(uint8_t product) {
  size_t i;

  if (product < SNFC_N74) {
    for (i = 0; i < sizeof(_product_info)
         / sizeof(struct _sproduct_info); i++) {
      if (_product_info[i].group == productGroup(product))
        return _product_info[i].chip_name;
    }
  } else {
    for (i = 0; i < sizeof(_product_info)
         / sizeof(struct _sproduct_info); i++) {
      if (_product_info[i].group == product)
        return _product_info[i].chip_name;
    }
  }
  return "Unknown product";
}

uint8_t get_product_support_fw(uint8_t product) {
  size_t i;

  if (product < SNFC_N74) {
    for (i = 0; i < sizeof(_product_info)
         / sizeof(struct _sproduct_info); i++) {
      if (_product_info[i].group == productGroup(product))
        return _product_info[i].major_version;
    }
  } else {
    for (i = 0; i < sizeof(_product_info)
         / sizeof(struct _sproduct_info); i++) {
      if (_product_info[i].group == product)
        return _product_info[i].major_version;
    }
  }

  return SNFC_UNKNOWN;
}
