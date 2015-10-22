/*
 * Copyright 2013-2015 Fabian Groffen
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

/**
 * 32-bits unsigned FNV1a returning into hash, using p to as variable to
 * walk over metric up to firstspace
 */
#define fnv1a_32(hash, p, metric, firstspace) \
	hash = 2166136261UL; \
	for (p = metric; p < firstspace; p++) \
		hash = (hash ^ (unsigned int)*p) * 16777619;
