/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ECMA_BIGINT_OBJECT_H
#define ECMA_BIGINT_OBJECT_H

#include "ecma-globals.h"

#if JERRY_BUILTIN_BIGINT

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabigintobject ECMA BigInt object related routines
 * @{
 */

ecma_value_t ecma_op_create_bigint_object(ecma_value_t arg);

/**
 * @}
 * @}
 */

#endif /* JERRY_BUILTIN_BIGINT */

#endif /* !ECMA_BIGINT_OBJECT_H */
