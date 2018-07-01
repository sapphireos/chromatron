/**
 * \file
 *
 * \brief Top level header file
 *
 * Copyright (c) 2018 Atmel Corporation, a wholly owned subsidiary of Microchip Technology Inc.
 *
 * \license_start
 *
 * \page License
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * \license_stop
 *
 */

#ifndef _SAM_
#define _SAM_

#if defined(__SAMS70J19__) || defined(__ATSAMS70J19__)
#include "sams70j19.h"
#elif defined(__SAMS70J20__) || defined(__ATSAMS70J20__)
#include "sams70j20.h"
#elif defined(__SAMS70J21__) || defined(__ATSAMS70J21__)
#include "sams70j21.h"
#elif defined(__SAMS70N19__) || defined(__ATSAMS70N19__)
#include "sams70n19.h"
#elif defined(__SAMS70N20__) || defined(__ATSAMS70N20__)
#include "sams70n20.h"
#elif defined(__SAMS70N21__) || defined(__ATSAMS70N21__)
#include "sams70n21.h"
#elif defined(__SAMS70Q19__) || defined(__ATSAMS70Q19__)
#include "sams70q19.h"
#elif defined(__SAMS70Q20__) || defined(__ATSAMS70Q20__)
#include "sams70q20.h"
#elif defined(__SAMS70Q21__) || defined(__ATSAMS70Q21__)
#include "sams70q21.h"
#else
#error Library does not support the specified device
#endif

#endif /* _SAM_ */
