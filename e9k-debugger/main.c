/*
 * COPYRIGHT © 2026 Enable Software Pty Ltd - All Rights Reserved
 *
 * https://github.com/alpine9000/engine9000-public
 *
 * See COPYING for license details
 */

#include "debugger.h"
#include "cli.h"

int
#ifdef _WIN32
SDL_main(int argc, char **argv)
#else
main(int argc, char **argv)
#endif
{
  int rc = 0;
  int loadTestTempConfig = 0;
  int testRestartCount = 0;
  cli_setArgv0((argc > 0 && argv) ? argv[0] : NULL);
  do {
    debugger_setLoadTestTempConfig(loadTestTempConfig);
    debugger_setTestRestartCount(testRestartCount);
    rc = debugger_main(argc, argv);
    if (rc == 2) {
      loadTestTempConfig = 1;
      testRestartCount++;
    } else {
      loadTestTempConfig = 0;
    }
  } while (rc == 2);
  return rc;
}
