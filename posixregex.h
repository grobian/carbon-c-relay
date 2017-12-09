/*
 * Copyright 2013-2017 Fabian Groffen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef HAVE_POSIXREGEX_H
#define HAVE_POSIXREGEX_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined (HAVE_ONIGURAMA)
#include "onigposix.h"
#elif defined (HAVE_PCRE2)
#include "pcre2posix.h"
#elif defined (HAVE_PCRE)
#include "pcreposix.h"
#else
#include <regex.h>
#endif

#endif
