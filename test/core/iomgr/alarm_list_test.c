/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "src/core/iomgr/alarm.h"

#include <string.h>

#include "src/core/iomgr/alarm_internal.h"
#include <grpc/support/log.h>
#include "test/core/util/test_config.h"

#define MAX_CB 30

static int cb_called[MAX_CB][2];
static int kicks;

void grpc_kick_poller(void) { ++kicks; }

static void cb(void *arg, int success) {
  cb_called[(gpr_intptr)arg][success]++;
}

static void add_test(void) {
  gpr_timespec start = gpr_now(GPR_CLOCK_REALTIME);
  int i;
  grpc_alarm alarms[20];

  grpc_alarm_list_init(start);
  memset(cb_called, 0, sizeof(cb_called));

  /* 10 ms alarms.  will expire in the current epoch */
  for (i = 0; i < 10; i++) {
    grpc_alarm_init(&alarms[i],
                    gpr_time_add(start, gpr_time_from_millis(10, GPR_TIMESPAN)),
                    cb, (void *)(gpr_intptr)i, start);
  }

  /* 1010 ms alarms.  will expire in the next epoch */
  for (i = 10; i < 20; i++) {
    grpc_alarm_init(&alarms[i], gpr_time_add(start, gpr_time_from_millis(
                                                        1010, GPR_TIMESPAN)),
                    cb, (void *)(gpr_intptr)i, start);
  }

  /* collect alarms.  Only the first batch should be ready. */
  GPR_ASSERT(10 == grpc_alarm_check(NULL,
                                    gpr_time_add(start, gpr_time_from_millis(
                                                            500, GPR_TIMESPAN)),
                                    NULL));
  for (i = 0; i < 20; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 10));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  GPR_ASSERT(0 == grpc_alarm_check(
                      NULL, gpr_time_add(
                                start, gpr_time_from_millis(600, GPR_TIMESPAN)),
                      NULL));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 10));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  /* collect the rest of the alarms */
  GPR_ASSERT(
      10 == grpc_alarm_check(NULL, gpr_time_add(start, gpr_time_from_millis(
                                                           1500, GPR_TIMESPAN)),
                             NULL));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 20));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  GPR_ASSERT(0 == grpc_alarm_check(NULL,
                                   gpr_time_add(start, gpr_time_from_millis(
                                                           1600, GPR_TIMESPAN)),
                                   NULL));
  for (i = 0; i < 30; i++) {
    GPR_ASSERT(cb_called[i][1] == (i < 20));
    GPR_ASSERT(cb_called[i][0] == 0);
  }

  grpc_alarm_list_shutdown();
}

static gpr_timespec tfm(int m) {
  gpr_timespec t = gpr_time_from_millis(m, GPR_TIMESPAN);
  t.clock_type = GPR_CLOCK_REALTIME;
  return t;
}

/* Cleaning up a list with pending alarms. */
void destruction_test(void) {
  grpc_alarm alarms[5];

  grpc_alarm_list_init(gpr_time_0(GPR_CLOCK_REALTIME));
  memset(cb_called, 0, sizeof(cb_called));

  grpc_alarm_init(&alarms[0], tfm(100), cb, (void *)(gpr_intptr)0,
                  gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_alarm_init(&alarms[1], tfm(3), cb, (void *)(gpr_intptr)1,
                  gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_alarm_init(&alarms[2], tfm(100), cb, (void *)(gpr_intptr)2,
                  gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_alarm_init(&alarms[3], tfm(3), cb, (void *)(gpr_intptr)3,
                  gpr_time_0(GPR_CLOCK_REALTIME));
  grpc_alarm_init(&alarms[4], tfm(1), cb, (void *)(gpr_intptr)4,
                  gpr_time_0(GPR_CLOCK_REALTIME));
  GPR_ASSERT(1 == grpc_alarm_check(NULL, tfm(2), NULL));
  GPR_ASSERT(1 == cb_called[4][1]);
  grpc_alarm_cancel(&alarms[0]);
  grpc_alarm_cancel(&alarms[3]);
  GPR_ASSERT(1 == cb_called[0][0]);
  GPR_ASSERT(1 == cb_called[3][0]);

  grpc_alarm_list_shutdown();
  GPR_ASSERT(1 == cb_called[1][0]);
  GPR_ASSERT(1 == cb_called[2][0]);
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);
  add_test();
  destruction_test();
  return 0;
}
