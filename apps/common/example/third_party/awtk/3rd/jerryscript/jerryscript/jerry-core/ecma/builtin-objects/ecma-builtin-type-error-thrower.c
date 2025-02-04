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

#include "ecma-alloc.h"
#include "ecma-builtins.h"
#include "ecma-conversion.h"
#include "ecma-exceptions.h"
#include "ecma-gc.h"
#include "ecma-globals.h"
#include "ecma-helpers.h"
#include "ecma-objects.h"

#include "jrt.h"

#define ECMA_BUILTINS_INTERNAL
#include "ecma-builtins-internal.h"

#define BUILTIN_INC_HEADER_NAME "ecma-builtin-type-error-thrower.inc.h"
#define BUILTIN_UNDERSCORED_ID  type_error_thrower
#include "ecma-builtin-internal-routines-template.inc.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmabuiltins
 * @{
 *
 * \addtogroup type_error_thrower ECMA [[ThrowTypeError]] object built-in
 * @{
 */

/**
 * Handle calling [[Call]] of built-in [[ThrowTypeError]] object
 *
 * See also:
 *          ECMA-262 v5, 13.2.3
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_type_error_thrower_dispatch_call(const ecma_value_t *arguments_list_p,  /**< arguments list */
        uint32_t arguments_list_len) /**< number of arguments */
{
    JERRY_ASSERT(arguments_list_len == 0 || arguments_list_p != NULL);

    return ecma_raise_type_error(ECMA_ERR_MSG("'caller', 'callee', and 'arguments' properties may not be accessed"
                                 " on strict mode functions or the arguments objects for calls to them"));
} /* ecma_builtin_type_error_thrower_dispatch_call */

/**
 * Handle calling [[Construct]] of built-in [[ThrowTypeError]] object
 *
 * See also:
 *          ECMA-262 v5, 13.2.3
 *
 * @return ecma value
 */
ecma_value_t
ecma_builtin_type_error_thrower_dispatch_construct(const ecma_value_t *arguments_list_p,  /**< arguments list */
        uint32_t arguments_list_len) /**< number of arguments */
{
    JERRY_ASSERT(arguments_list_len == 0 || arguments_list_p != NULL);

    return ecma_builtin_type_error_thrower_dispatch_call(arguments_list_p, arguments_list_len);
} /* ecma_builtin_type_error_thrower_dispatch_construct */

/**
 * @}
 * @}
 * @}
 */
