/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file timer.c
 * @brief Linux implementation of the timer interface.
 */

#include <stddef.h>
#include <windows.h>


#include "timer_windows.h"

char expired(Timer* timer) {
	DWORD curr_time = GetTickCount();
	return curr_time > timer->timeout_time;
}

void countdown_ms(Timer* timer, unsigned int timeout) {
	timer->timeout_time = GetTickCount() + timeout;
}

void countdown(Timer* timer, unsigned int timeout) {
	timer->timeout_time = GetTickCount() + timeout * 1000;
}

int left_ms(Timer* timer) {
	int elapsed = timer->timeout_time - GetTickCount();
	return max( elapsed, 0 );
}

void InitTimer(Timer* timer) {
	timer->timeout_time = 0;
}
