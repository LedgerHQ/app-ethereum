/**
  ******************************************************************************
  * @file    usbd_customhid_if_template.h
  * @author  MCD Application Team
  * @version V2.2.0
  * @date    13-June-2014
  * @brief   Header for usbd_customhid_if_template.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for th?
  e specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CUSTOM_HID_IF_TEMPLATE_H
#define __USBD_CUSTOM_HID_IF_TEMPLATE_H

#define USBD_CUSTOMHID_REPORT_DESC_SIZE 0x22
#define USBD_CUSTOMHID_OUTREPORT_BUF_SIZE 0x40

#define USBD_CUSTOM_HID_REPORT_DESC_SIZE 0x22
#define USBD_CUSTOM_HID_OUTREPORT_BUF_SIZE 0x40

/* Includes ------------------------------------------------------------------*/
#include "usbd_customhid.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
extern USBD_CUSTOM_HID_ItfTypeDef const USBD_CustomHID_template_fops;

#endif /* __USBD_CUSTOM_HID_IF_TEMPLATE_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
