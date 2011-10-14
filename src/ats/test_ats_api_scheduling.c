/*
     This file is part of GNUnet.
     (C) 2010,2011 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/
/**
 * @file ats/test_ats_api_scheduling.c
 * @brief test automatic transport selection scheduling API
 * @author Christian Grothoff
 * @author Matthias Wachs
 *
 * TODO:
 * - write test case
 * - extend API to get performance data
 * - implement simplistic strategy based on say 'lowest latency' or strict ordering
 * - extend API to get peer preferences, implement proportional bandwidth assignment
 * - re-implement API against a real ATS service (!)
 */
#include "platform.h"
#include "gnunet_ats_service.h"
#include "ats.h"

#define VERBOSE GNUNET_EXTRA_LOGGING

#define VERBOSE_ARM GNUNET_EXTRA_LOGGING

#define TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 10)

static GNUNET_SCHEDULER_TaskIdentifier die_task;

static struct GNUNET_ATS_SchedulingHandle *ats;

struct GNUNET_OS_Process * arm_proc;

static int ret;

struct Address
{
  char * plugin;
  size_t plugin_len;

  void * addr;
  size_t addr_len;

  struct GNUNET_TRANSPORT_ATS_Information * ats;
  int ats_count;

  void  *session;
};

struct PeerContext
{
  struct GNUNET_PeerIdentity id;

  struct Address * addr;
};


static void
stop_arm ()
{
  if (0 != GNUNET_OS_process_kill (arm_proc, SIGTERM))
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_WARNING, "kill");
  GNUNET_OS_process_wait (arm_proc);
  GNUNET_OS_process_close (arm_proc);
  arm_proc = NULL;
}


static void
end_badly (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  die_task = GNUNET_SCHEDULER_NO_TASK;
  if (ats != NULL)
    GNUNET_ATS_scheduling_done (ats);

  ret = GNUNET_SYSERR;

  stop_arm ();
}


static void
end ()
{
  if (die_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel(die_task);
    die_task = GNUNET_SCHEDULER_NO_TASK;
  }

  GNUNET_ATS_scheduling_done (ats);

  ret = 0;

  stop_arm ();
}


static void
address_suggest_cb (void *cls,
                    const struct
                    GNUNET_PeerIdentity *
                    peer,
                    const char *plugin_name,
                    const void *plugin_addr,
                    size_t plugin_addr_len,
                    struct Session * session,
                    struct
                    GNUNET_BANDWIDTH_Value32NBO
                    bandwidth_out,
                    struct
                    GNUNET_BANDWIDTH_Value32NBO
                    bandwidth_in,
                    const struct
                    GNUNET_TRANSPORT_ATS_Information
                    * ats,
                    uint32_t ats_count)

{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "address_suggest_cb `%s'\n", GNUNET_i2s (peer));

  end ();
}

void
start_arm (const char *cfgname)
{
  arm_proc = GNUNET_OS_start_process (NULL, NULL, "gnunet-service-arm",
                           "gnunet-service-arm",
#if VERBOSE_ARM
                           "-L", "DEBUG",
#endif
                           "-c", cfgname, NULL);
}

static void
check (void *cls, char *const *args, const char *cfgfile,
       const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  ret = GNUNET_SYSERR;
  struct Address addr;
  struct PeerContext p;

  die_task = GNUNET_SCHEDULER_add_delayed(TIMEOUT, &end_badly, NULL);
  start_arm (cfgfile);

  ats = GNUNET_ATS_scheduling_init (cfg, &address_suggest_cb, NULL);

  if (ats == NULL)
  {
    ret = GNUNET_SYSERR;
    end ();
    return;
  }

  /* set up peer */
  GNUNET_CRYPTO_hash_create_random(GNUNET_CRYPTO_QUALITY_WEAK, &p.id.hashPubKey);


  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Created peer `%s'\n", GNUNET_i2s (&p.id));
  p.addr = &addr;
  addr.plugin = "test";
  addr.session = NULL;
  addr.addr = NULL;
  addr.addr_len = 0;

  GNUNET_ATS_address_update(ats, &p.id, addr.plugin, addr.addr, addr.addr_len, addr.session, NULL, 0);

  GNUNET_ATS_suggest_address(ats, &p.id);
}

int
main (int argc, char *argv[])
{
  static char *const argv2[] = { "test_ats_api_scheduling",
    "-c",
    "test_ats_api.conf",
#if VERBOSE
    "-L", "DEBUG",
#else
    "-L", "WARNING",
#endif
    NULL
  };

  static struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_OPTION_END
  };

  GNUNET_PROGRAM_run ((sizeof (argv2) / sizeof (char *)) - 1, argv2,
                      "test_ats_api_scheduling", "nohelp", options, &check,
                      NULL);


  return ret;
}

/* end of file test_ats_api_scheduling.c */
