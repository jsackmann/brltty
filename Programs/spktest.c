/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 1995-2015 by The BRLTTY Developers.
 *
 * BRLTTY comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/
 *
 * This software is maintained by Dave Mielke <dave@mielke.cc>.
 */

/* spktest.c - Test progrm for the speech synthesizer drivers.
 */

#include "prologue.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "program.h"
#include "options.h"
#include "spk.h"
#include "log.h"
#include "file.h"
#include "parse.h"

volatile SpeechSynthesizer spk;

static char *opt_textString;
static char *opt_speechVolume;
static char *opt_speechRate;
char *opt_pcmDevice;
static char *opt_driversDirectory;

BEGIN_OPTION_TABLE(programOptions)
  { .letter = 'D',
    .word = "drivers-directory",
    .flags = OPT_Hidden,
    .argument = "directory",
    .setting.string = &opt_driversDirectory,
    .internal.setting = DRIVERS_DIRECTORY,
    .internal.adjust = fixInstallPath,
    .description = "Path to directory for loading drivers."
  },

  { .letter = 't',
    .word = "text-string",
    .argument = "string",
    .setting.string = &opt_textString,
    .description = "Text to be spoken."
  },

  { .letter = 'v',
    .word = "volume",
    .argument = "loudness",
    .setting.string = &opt_speechVolume,
    .description = "Floating-point speech volume multiplier."
  },

  { .letter = 'r',
    .word = "rate",
    .argument = "speed",
    .setting.string = &opt_speechRate,
    .description = "Floating-point speech rate multiplier."
  },

  { .letter = 'd',
    .word = "device",
    .argument = "device",
    .setting.string = &opt_pcmDevice,
    .description = "Digital audio soundcard device specifier."
  },
END_OPTION_TABLE

static int
sayLine (char *line, void *data) {
  sayString(line, 0);
  return 1;
}

int
main (int argc, char *argv[]) {
  ProgramExitStatus exitStatus;

  const char *driver = NULL;
  void *object;

  float speechVolume = 1.0;
  float speechRate = 1.0;

  {
    static const OptionsDescriptor descriptor = {
      OPTION_TABLE(programOptions),
      .applicationName = "spktest",
      .argumentsSummary = "[driver [parameter=value ...]]"
    };

    PROCESS_OPTIONS(descriptor, argc, argv);
  }

  if (opt_speechVolume && *opt_speechVolume) {
    static const float minimum = 0.0;
    static const float maximum = 2.0;

    if (!validateFloat(&speechVolume, opt_speechVolume, &minimum, &maximum)) {
      logMessage(LOG_ERR, "%s: %s", "invalid volume multiplier", opt_speechVolume);
      return PROG_EXIT_SYNTAX;
    }
  }

  if (opt_speechRate && *opt_speechRate) {
    static const float minimum = 0.1;
    static const float maximum = 10.0;

    if (!validateFloat(&speechRate, opt_speechRate, &minimum, &maximum)) {
      logMessage(LOG_ERR, "%s: %s", "invalid rate multiplier", opt_speechRate);
      return PROG_EXIT_SYNTAX;
    }
  }

  if (argc) {
    driver = *argv++, --argc;
  }

  if ((speech = loadSpeechDriver(driver, &object, opt_driversDirectory))) {
    const char *const *parameterNames = speech->parameters;
    char **parameterSettings;

    if (!parameterNames) {
      static const char *const noNames[] = {NULL};

      parameterNames = noNames;
    }

    {
      const char *const *name = parameterNames;
      unsigned int count;
      char **setting;

      while (*name) name += 1;
      count = name - parameterNames;

      if (!(parameterSettings = malloc((count + 1) * sizeof(*parameterSettings)))) {
        logMallocError();
        return PROG_EXIT_FATAL;
      }

      setting = parameterSettings;
      while (count--) *setting++ = "";
      *setting = NULL;
    }

    while (argc) {
      char *assignment = *argv++;
      int ok = 0;
      char *delimiter = strchr(assignment, '=');

      if (!delimiter) {
        logMessage(LOG_ERR, "missing speech driver parameter value: %s", assignment);
      } else if (delimiter == assignment) {
        logMessage(LOG_ERR, "missing speech driver parameter name: %s", assignment);
      } else {
        size_t nameLength = delimiter - assignment;
        const char *const *name = parameterNames;

        while (*name) {
          if (strncasecmp(assignment, *name, nameLength) == 0) {
            parameterSettings[name - parameterNames] = delimiter + 1;
            ok = 1;
            break;
          }

          name += 1;
        }

        if (!ok) logMessage(LOG_ERR, "invalid speech driver parameter: %s", assignment);
      }

      if (!ok) return PROG_EXIT_SYNTAX;
      argc -= 1;
    }

    constructSpeechSynthesizer(&spk);
    identifySpeechDriver(speech, 0);		/* start-up messages */

    if (startSpeechDriverThread(parameterSettings)) {
      if (canSetSpeechVolume()) setSpeechVolume(speechVolume, 0);
      if (canSetSpeechRate()) setSpeechRate(speechRate, 0);

      if (opt_textString && *opt_textString) {
        sayString(opt_textString, 0);
      } else {
        processLines(stdin, sayLine, NULL);
      }

      stopSpeechDriverThread();		/* finish with the display */
      exitStatus = PROG_EXIT_SUCCESS;
    } else {
      logMessage(LOG_ERR, "can't initialize speech driver");
      exitStatus = PROG_EXIT_FATAL;
    }
  } else {
    logMessage(LOG_ERR, "can't load speech driver");
    exitStatus = PROG_EXIT_FATAL;
  }

  return exitStatus;
}
