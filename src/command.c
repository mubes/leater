/*
 * Handler for all command interaction. Parses commands from the uart (or other source)
 * and dispatches events to the correct module via the various COMMAND(x) routines.
 */

#include <ctype.h>
#include <string.h>
#include "config.h"
#include "command.h"
#include "uart.h"
#include "nv.h"
#include "statemachine.h"
#include "printf.h"
#include "log.h"
#include "bod.h"
#include "profile.h"
#include "timers.h"

// ============================================================================================
#define MAX_PARAMS 8 // Maximum number of parameters to be passed in any routine
#define COMMAND(x) BOOL (x)(uint32_t nparams, char **param) // A command handler
#define VARPARAM 127        // Flag for a variable number of parameters
// These are VT100 codes ... they are reasonably universal
#define BEEP     7              // Output a beep
#define BACKDEL  "\b \b"        // Delete backwards
#define ERASEEOL "\033[K"       // Erase to end of line
#define ERASESCREEN "\033[2J"   // Erase entire screen
#define CURSORON "\033[?25h"    // Show cursor
#define CURSOROFF  "\033[?25l"  // Hide cursor
typedef struct
{
    char *command;
    uint8_t nparams;
    COMMAND(*handler);
} _commandList;

static timerType t;                 // For refreshing the display
static BOOL paramsModified;         // Have parameters been modified and not comitted?

const char * const sysConfigStrings[] =
{ LEATER_CONFIG_STRINGLIST };

#define CL_REFRESH_INTERVAL     (1*mS)
// ============================================================================================
// Internal routines
// ============================================================================================
static uint32_t _datoi(char *p)

// Turn a ASCII into an integer

{
    int32_t neg = 1;
    uint32_t retval = 0;

    if (*p == '-')
        {
            p++;
            neg = -1;
        }

    while (isdigit(*p))
        retval = retval * 10 + *p++ - '0';

    return retval * neg;
}
// ============================================================================================
uint32_t _datosf(char *p, uint32_t scale)

// Turn an ASCII into a fixed-point number with defined scale

{
    uint32_t intpart;

    intpart = _datoi(p);

    while ((*p != '.') && (*p))
        p++;

    // If there's a decimal, skip past it
    if (*p) p++;

    while (scale > 1)
        {
            intpart = intpart * 10;
            if (*p) intpart += *p++ - '0';
            scale /= 10;
        }

    return intpart;
}
// ============================================================================================
#define _dtoupper(x) (((x>='a') && (x<='z'))?x-('a'-'A'):x)
// ============================================================================================
int32_t _dstrcasecmp(const char *s1, const char *s2)

// Case insensitive comparison

{
    while ((_dtoupper(*s1) == _dtoupper(*s2))&& (*s1))
        {
            s1++;
            s2++;
        };
    if (*s1==*s2) return 0;
    if (_dtoupper(*s1)<_dtoupper(*s2)) return -1;
    return 1;
}
// ============================================================================================
int32_t _dstrcaseacmp(char *s1, char *s2)

// Case insensitve comparison, up to length of first string

{
    while ((_dtoupper(*s1) == _dtoupper(*s2))&& (*s1))
        {
            s1++;
            s2++;
        };
    if (!*s1) return 0;
    if (_dtoupper(*s1)<_dtoupper(*s2)) return -1;
    return 1;
}
// ============================================================================================
// Handlers for the various comments
// ============================================================================================
// ============================================================================================
COMMAND(_flushlogs)

// Flush out the logs, and tell the state machine that we did it

{
    logFlushLogs();
    stateLogsFlushed();
    return TRUE;
}
// ============================================================================================
COMMAND(_loginfo)

// Get information about the state of the logs

{
    logIterator n;
    uint32_t entryCount = 0;
    uint32_t logNumber = 0;
    BOOL noMoreData;

    logInitIterator(&n);
    do
        {
            noMoreData = logIteratorNext(&n);
            if (logNumber != logCurrentLog(&n))
                {
                    // This is a new log entry - so if we've already got one then report it
                    if (logNumber) commandprintf("%3d:%5d entries\n", logNumber, entryCount);
                    logNumber++;
                    entryCount = 0;
                }
            else
                entryCount++;
        }
    while (noMoreData);
    if (logNumber)
        commandprintf("%3d:%5d entries %s\n", logNumber, entryCount,
                      (logNumber == logNumLogs()) ? "(Still Active)" : "");

    commandprintf("Current log number is       : %d\n", logNumLogs());
    commandprintf("Number of logs with records : %d (inc. active one)\n", logNumber);
    commandprintf("Amount of remaining space   : %d Bytes (%d%%)\n", nvGetSpace(),
                  (nvGetSpace() * 100) / nvTotalSpace());

    return TRUE;
}
// ============================================================================================
COMMAND(_newlog)

// Create a new log record

{
    if (logNewLog())
        {
            commandprintf("New log started, number %d\n", logNumLogs());
            return TRUE;
        }

    return FALSE;
}
// ============================================================================================
COMMAND(_pidmonitor)

// Switch pid monitoring on or off

{
    if (!_dstrcasecmp((char *) param[1], "ON"))
        {
            statePIDMonitor(TRUE);
            return TRUE;
        }
    if (!_dstrcasecmp((char *) param[1], "OFF"))
        {
            statePIDMonitor(FALSE);
            return TRUE;
        }

    return FALSE;
}
// ============================================================================================
COMMAND(_internal_setparam)

// Set a specific parameter in the configuration memory. Note that sets are not permanent
// until they are committed.

{
    if (nparams < 2) commandprintf("Wrong number of parameters\n");
    config_item_enum c = 0;
    while ((c < CONFIG_ITEM_end) && (_dstrcasecmp(sysConfigStrings[c], (char *) param[1])))
        c++;

    if (c == CONFIG_ITEM_end) return FALSE;

    // We got a match, so process it
    switch (c)
        {
            case CONFIG_ITEM_version:
                commandprintf("Value cannot be changed\n");
                return FALSE;
                // ---------------------------
            case CONFIG_ITEM_logOutputCSV:
                if (nparams < 2)
                    {
                        commandprintf("Wrong number of parameters\n");
                        return FALSE;
                    }
                if ((_dstrcasecmp("1", param[2]) == 0) || (_dstrcasecmp("TRUE", param[2]) == 0))
                    {
                        sysConfig.logOutputCSV = TRUE;
                        commandprintf("CSV output on\n");
                        return TRUE;
                    }
                if ((_dstrcasecmp("0", param[2]) == 0) || (_dstrcasecmp("FALSE", param[2]) == 0))
                    {
                        sysConfig.logOutputCSV = FALSE;
                        commandprintf("CSV output off (pretty printed output enabled)\n");
                        return TRUE;
                    }
                return FALSE;
                // -----------------------
            case CONFIG_ITEM_setPoint:
                if (nparams < 2)
                    {
                        commandprintf("Wrong number of parameters\n");
                        return FALSE;
                    }
                uint32_t setpointSet;
                setpointSet = _datosf(param[2], DEGREE);
                sysConfig.setPoint = setpointSet;
                if (sysConfig.defaultProfile == SETPOINT_STATIC)
                    {
                        stateChangeSetpoint(setpointSet);
                        commandprintf("SetPoint is set to %d.%d°C\n", setpointSet / DEGREE,
                                      (setpointSet % DEGREE * 10) / DEGREE);
                    }
                else
                    commandprintf("Pending setpoint is %d.%d°C (Not active)\n", setpointSet / DEGREE,
                                  (setpointSet % DEGREE * 10) / DEGREE);
                return TRUE;
                // ------------------------
            case CONFIG_ITEM_recordInterval:
                if (nparams < 2)
                    {
                        commandprintf("Wrong number of parameters\n");
                        return FALSE;
                    }
                uint32_t recordInterval;
                recordInterval = _datoi(param[2]);
                if (recordInterval > 0)
                    {
                        sysConfig.recordInterval = recordInterval;
                        commandprintf("Record interval set to %dS\n", recordInterval);
                        return TRUE;
                    }
                else
                    {
                        commandprintf("Illegal record interval\n");
                        return FALSE;
                    }
                // -----------------------
            case CONFIG_ITEM_PID:
                if (nparams != 5)
                    {
                        commandprintf("Syntax: PID <Kc> <tauI> <tauD>\n");
                        return FALSE;
                    }

                sysConfig.Cp   = _datoi(param[2]);
                sysConfig.Ci = _datoi(param[3]);
                sysConfig.Cd = _datoi(param[4]);
                statePIDSet();  // Let the state machine know there has been a change
                return TRUE;
                // -----------------------
            case CONFIG_ITEM_K:
                if (nparams < 2)
                    {
                        commandprintf("Wrong number of parameters\n");
                        return FALSE;
                    }

                sysConfig.k = _datoi(param[2]);
                return TRUE;
                // -----------------------
            case CONFIG_ITEM_defaultProfile:
                if (nparams != 3)
                    {
                        commandprintf("Syntax: defaultProfile <Num>|IDLE|STATIC\n");
                        return FALSE;
                    }

                uint32_t defaultProfileSet;

                if ((_dstrcaseacmp(param[2], "STATIC") == 0))
                    {
                        sysConfig.defaultProfile = SETPOINT_STATIC;
                        return TRUE;
                    }

                if ((_dstrcaseacmp(param[2], "IDLE") == 0))
                    {
                        sysConfig.defaultProfile = SETPOINT_IDLE;
                        return TRUE;
                    }

                defaultProfileSet = _datoi(param[2]);

                if ((defaultProfileSet <1) || (defaultProfileSet > MAX_PROFILES))
                    {
                        commandprintf("Profile must be in range 1-%d\n", MAX_PROFILES);
                        return FALSE;
                    }

                sysConfig.defaultProfile = defaultProfileSet;
                return TRUE;

                // -----------------------
            case CONFIG_ITEM_defaults:
                commandprintf("Defaults loaded\n");
                ConfigStoreType defaultSysConfig =
                { LEATER_CONFIG };     // This needs to be done here because string init cannot be done at global level
                memcpy(&sysConfig, &defaultSysConfig, sizeof(&defaultSysConfig));
                statePIDSet();  // Let the state machine know there has been a change
                return TRUE;
                // ---------------------
            case CONFIG_ITEM_reload:
                nvReadConfig(&sysConfig);
                commandprintf("Stored config reloaded\n");
                statePIDSet();  // Let the state machine know there has been a change
                return TRUE;
                // -------------------
            case CONFIG_ITEM_help:
                c = 0;
                uint32_t slen;
                while (c < CONFIG_ITEM_end)
                    {
                        commandprintf("%s", sysConfigStrings[c]);
                        slen = strlen(sysConfigStrings[c]);
                        while (slen++ < 15)
                            commandprintf(" ");

                        if (!(c % 5)) commandprintf("\n");
                        c++;
                    }
                commandprintf("\n");
                return TRUE;
                // -----
            default:
                commandprintf("Unknown set variable\n");
                return FALSE;
        }
}
// ============================================================================================
COMMAND(_setparam)

// Internal redirector to capture if changes where made

{
    BOOL retVal = _internal_setparam(nparams, param);
    paramsModified |= retVal;
    return retVal;
}
// ============================================================================================
COMMAND(_setpoint)

// Change temperature setpoint - overrides all other setpoint requests (e.g. from the profile
// manager) until the next one arrives.

{
    uint32_t setpointSet;

    if (_dstrcaseacmp("IDLE", param[1]) == 0)
        {
            stateChangeSetpoint(SETPOINT_IDLE);
            return TRUE;
        }
    setpointSet = _datosf(param[1], DEGREE);
    stateChangeSetpoint(setpointSet);
    return TRUE;
}
// ============================================================================================
COMMAND(_output)

// Change output level (if in open loop mode)

{
    uint32_t outputSet;

    if (stateAutomatic())
        {
            commandprintf("Not running open loop!\n");
            return FALSE;
        }

    outputSet = _datoi(param[1]);
    stateSetOutputLevel(outputSet);
    return TRUE;
}
// ============================================================================================
COMMAND(_dumpparam)

// Dump the current set of in-memory parameters....these may not be the same as the stored parameters.

{
    uint32_t profileNum = 0;
    uint32_t profileStep;

    commandprintf("[%s] Config Version        : 0x%08X\n", sysConfigStrings[CONFIG_ITEM_version],
                  sysConfig.version);
    commandprintf("[%s] Output CSV       : %s\n", sysConfigStrings[CONFIG_ITEM_logOutputCSV],
                  sysConfig.logOutputCSV ? "TRUE" : "FALSE");
    commandprintf("[%s] Target Temp          : %d.%d°C\n", sysConfigStrings[CONFIG_ITEM_setPoint],
                  sysConfig.setPoint / DEGREE,
                  (sysConfig.setPoint % DEGREE * 10) / DEGREE);
    commandprintf("[%s] Parameters 		: Cp=%d Ci=%d Cd=%d\n", sysConfigStrings[CONFIG_ITEM_PID],
                  sysConfig.Cp, sysConfig.Ci,
                  sysConfig.Cd);
    commandprintf("[%s] LowPass 	                : %d\n", sysConfigStrings[CONFIG_ITEM_K], sysConfig.k);
    commandprintf("[%s] Report interval: %d S\n", sysConfigStrings[CONFIG_ITEM_recordInterval],
                  sysConfig.recordInterval);
    commandprintf("[%s] Autorun        : ", sysConfigStrings[CONFIG_ITEM_defaultProfile]);
    if (sysConfig.defaultProfile <= MAX_PROFILES) commandprintf("%d\n", sysConfig.defaultProfile);
    else if (sysConfig.defaultProfile == SETPOINT_IDLE) commandprintf("IDLE\n");
    else if (sysConfig.defaultProfile == SETPOINT_STATIC) commandprintf("STATIC\n");
    else
        commandprintf("??????\n");
    if (paramsModified) commandprintf("Not ");
    commandprintf("Committed to NV\n");

    commandprintf("Profiles....\n");
    do
        {
            commandprintf("%d: %s;\n", profileNum + 1, sysConfig.profile[profileNum].name);
            profileStep = 0;
            do
                {
                    commandprintf("    %d %s %3d°C in %3d seconds\n", profileStep + 1,
                                  profileGetStepname((sysConfig.profile[profileNum].step[profileStep].command & PROFILE_COMMAND_MASK)
                                                     >> 28),
                                  ((sysConfig.profile[profileNum].step[profileStep].temp) & PROFILE_TEMP_MASK) / DEGREE,
                                  (sysConfig.profile[profileNum].step[profileStep].time) / mS);
                }
            while ((++profileStep < MAX_PROFILE_STEPS)
                    && ((sysConfig.profile[profileNum].step[profileStep].command & PROFILE_COMMAND_MASK) !=
                        PROFILE_END));
        }
    while (++profileNum < MAX_PROFILES);

    return TRUE;
}
// ============================================================================================
COMMAND(_commit)

// Commit the current in-memory parameters to non-volatile storage, and print them out too for good measure.

{
    if (!nvWriteConfig(&sysConfig)) return FALSE;
    paramsModified = FALSE;

    return _dumpparam(nparams, param);
}
// ============================================================================================
COMMAND(_profilemonitor)

// Switch profile monitoring on or off

{
    if (!_dstrcasecmp((char *) param[1], "ON"))
        {
            profileTraceOn(TRUE);
            return TRUE;
        }
    if (!_dstrcasecmp((char *) param[1], "OFF"))
        {
            profileTraceOn(FALSE);
            return TRUE;
        }

    return FALSE;
}
// ============================================================================================
COMMAND(_loop)

// Switch between open and closed loop mode

{
    if (!_dstrcasecmp((char *) param[1], "OPEN"))
        {
            stateLoopOpen(TRUE);
            return TRUE;
        }
    if (!_dstrcasecmp((char *) param[1], "CLOSE"))
        {
            stateLoopOpen(FALSE);
            return TRUE;
        }

    return FALSE;
}
// ============================================================================================
COMMAND(_info)

// Get info about the current configuration

{
    uint32_t part = nvRead_partVersion();

    commandprintf("Active log %d\n", logNumLogs());
    commandprintf("Version V%02d.%d\n", (part >> 8) & 0xFF, part & 0xFF);
    commandprintf("Variant %08x\n", nvRead_partID());
    commandprintf("Brownouts %d\n", bodCount());
#ifdef SENSOR_THERMISTOR
    commandprintf("Thermistor Sensor\n");
#endif
#ifdef SENSOR_THERMOCOUPLE
    commandprintf("Thermocouple\n");
#endif
    return TRUE;
}
// ============================================================================================
COMMAND(_uptime)

{
    commandprintf("%d Seconds\n",timerSecs());
    return TRUE;
}
// ============================================================================================
COMMAND(_run)

// Run a profile

{
    uint32_t profileToRun = _datoi(param[1]);

    if (!profileIsIdle())
        {
            commandprintf("Profile already active\n");
            return FALSE;
        }

    if ((!profileToRun) || (profileToRun > MAX_PROFILES))
        {
            commandprintf("Profile in range 1-%d\n", MAX_PROFILES);
            return FALSE;
        }

    return profileRun(profileToRun - 1);
}
// ============================================================================================
COMMAND(_stop)

// Stop a profile

{
    return profileStop();
}
// ============================================================================================
COMMAND(_dumplog)

// Dump a specific log out

{
    logIterator n;
    uint32_t entryCount = 0, currentLog = 0, compareLog = 0;
    uint32_t logInterval = 0, logTime = 0, oldPct = 0, logSetpoint = 0;

    if (!strcmp((char *) param[1], "ALL")) compareLog = 0;
    else
        compareLog = _datoi(param[1]);

    logInitIterator(&n);
    while (logIteratorNext(&n))
        {
            if (currentLog != logCurrentLog(&n))
                {
                    if (((compareLog == 0) || (compareLog == currentLog)) && (currentLog != 0)
                            && (!sysConfig.logOutputCSV))
                        commandprintf("%d entries\n", entryCount);

                    currentLog = logCurrentLog(&n);
                    entryCount = 0;
                    oldPct = 0;
                    logTime = 0;
                    logInterval = 0;
                    if (((compareLog == 0) || (compareLog == currentLog)) && (!sysConfig.logOutputCSV))
                        commandprintf("\n\nLog Number %d\n", currentLog);

                    if ((sysConfig.logOutputCSV) && (currentLog == 1))
                        commandprintf("Log Number,Time (s),Temp (°C),On Time (%%),Setpoint (°C)\n");
                }
            if ((compareLog == 0) || (compareLog == currentLog))
                switch (logVariable(&n))
                    {
                        case LOG_TEMPERATURE:
                            logTime += logInterval;
                            if (sysConfig.logOutputCSV) commandprintf("%d,%d,%d.%d,%d,%d\n", currentLog, logTime,
                                        logValue(&n) / DEGREE,
                                        (logValue(&n) / 10) % DEGREE, oldPct, logSetpoint / DEGREE);
                            else
                                commandprintf("%7ds S:%d°C A:%d.%d°C %d%% On time\n", logTime, logSetpoint / DEGREE,
                                              logValue(&n) / DEGREE,
                                              ((logValue(&n) * 10) / DEGREE) % 10, oldPct);
                            break;

                        case LOG_ON_PERCENTAGE:
                            oldPct = logValue(&n);
                            break;

                        case LOG_RECORD_INTERVAL:
                            logInterval = logValue(&n);
                            break;

                        case LOG_SETPOINT_SET:
                            logSetpoint = logValue(&n);
                            break;

                        default:
                            commandprintf("%7ds %s: %d %s\n", logTime, logVariableName(&n), logValue(&n), logUnits(&n));
                            break;
                    }
            entryCount++;
        }
    return TRUE;
}
// ============================================================================================
void _doConnected(void)

// Version of the connected call with superfluous parameters removed

{
    // We got a connected event - so just send a welcome
    commandprintf("\n\n\n\n\rLeater Version %s\n", LEATER_VERSION);
}
// ============================================================================================
COMMAND(_connected)

// This originates from BT transponder if fitted...

{
    _doConnected();
    return TRUE;
}
// ============================================================================================
COMMAND(_help);     // A bit chicken and egg :-)

// Make sure these stay in alpha order - it makes searching and auto-completion a lot easier!

static const _commandList commands[] =
{
    { "+CONNECTED", 1, &_connected },
    { "Commit", 1, &_commit },
    { "Dumplog", 2, &_dumplog },
    { "Dumpparam", 1, &_dumpparam },
    { "Flushlogs", 1, &_flushlogs },
    { "Help", 1, &_help },
    { "Info", 1, &_info },
    { "Loginfo", 1, _loginfo },
    { "Loop", 2, _loop },
    { "Newlog", 1, _newlog },
    { "Output", 2, _output },
    { "PIDMonitor", 2, _pidmonitor },
    { "Profilemonitor", 2, _profilemonitor },
    { "Run", 2, _run },
    { "Setparam", VARPARAM, _setparam },
    { "Setpoint", 2, _setpoint },
    { "Stop", 1, _stop },
    { "Uptime", 1, _uptime },
    { 0, 0, 0 }
};

COMMAND(_help)

// Issue list of commands

{
    const _commandList *c = commands;
    uint32_t slen;

    c++;     // Skip the '+' commands

    while (c->handler)
        {
            commandprintf("%s", c->command);
            slen = strlen(c->command);
            while (slen++ < 15)
                commandprintf(" ");

            if (!((c - commands) % 5)) commandprintf("\n");
            c++;
        }
    commandprintf("\n");
    return TRUE;
}
// ============================================================================================
void _parse(void)

// Parse the entered line, breaking it into tokens, then dispatch it

{
    char *rxed = (char *) uartGets();
    char *param[MAX_PARAMS];
    uint8_t paramCount = 0;
    char *p = rxed;
    const _commandList *c = commands;

    if (!*rxed) return;

    while (*p)
        {
            while (isspace(*p))
                p++;
            if (*p)
                {
                    param[paramCount++] = p;     // end of whitespace - so squirrel it
                    while ((*p) && (!isspace(*p)))
                        p++;
                    if (*p) *p++ = 0;
                }
        }

    // Go through looking for a match
    while (((c->handler) && _dstrcasecmp((char *) c->command, (char *) param[0]) < 0))
        c++;

    if ((!c->handler) || (_dstrcasecmp((char *) c->command, (char *) param[0])))
        {
            commandprintf("Unrecognised command\n");
            return;
        }

    if ((c->nparams != paramCount) && (c->nparams != VARPARAM))
        {
            commandprintf("Wrong number of parameters\n");
            return;
        }

    if (!c->handler(paramCount, param)) commandprintf("Error\n");
}
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
void commandReportLine(char *fmt, ...)

// Print a line to the console without upsetting the display formatting

{
    uartPutsn("\r" ERASEEOL "\r");

    va_list va;
    va_start(va, fmt);
    vprintf(fmt, &va);
    va_end(va);
    commandRefreshPrompt();
}
// ============================================================================================
void commandprintf(char *fmt, ...)

// Handle simple print

{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, &va);
    va_end(va);
}
// ============================================================================================
void commandHandleException(uartExceptionType e)

// This is the callback from the UART character reception when something interesting has happened

{
    const _commandList *c = commands;

    switch (e & UART_EXCEPTION_MASK)
        {
                // --- A complete line has arrived
            case UART_LINE_RXED:
                printf("\n");
                _parse();
                uartUnlockbuffer();
                break;
                // ---------------------------------
            case UART_CHARRXED:
#ifdef USE_ECHO
                uartPutchar(e >> 24);
#endif
                break;

                // --- A control code has been entered - process the ones we handle
            case UART_CTRL_CODE:
                switch ((e >> 24) & 0xFF)
                    {
                            // ---------------------------------
                        case 3:     // CTRL-C
                            printf("\n");
                            uartUnlockbuffer();
                            break;
                            // ---------------------------------
                        case 12:     // CTRL-L (Redraw)
                            uartPutsn(ERASESCREEN);
                            commandRefreshPrompt();
                            break;
                            // ---------------------------------
                        case 9:     // Tab (Command completion)
                            // Go through looking for a match
                            while (((c->handler) && _dstrcaseacmp(c->command, uartGets()) < 0))
                                c++;
                            if ((c->handler) && (_dstrcaseacmp(uartGets(), (char *) c->command) == 0))
                                {
                                    // We got a match - see if there is more than one

                                    if (((c + 1)->handler) && (_dstrcaseacmp(uartGets(), (char *) (c + 1)->command) == 0))
                                        {
                                            printf("\n");
                                            while (((c->handler) && _dstrcaseacmp(uartGets(), c->command) == 0))
                                                {
                                                    uartPutsn(c->command);
                                                    uartPutsn("   ");
                                                    c++;
                                                }
                                            printf("%c\n", BEEP);
                                            commandRefreshPrompt();
                                        }
                                    else
                                        {
                                            // There is a match, and the next entry isn't a match - so we can complete
                                            while (c->command[strlen(uartGets())])
                                                uartInsertChar(c->command[strlen(uartGets())]);
                                            uartInsertChar(' ');
                                        }
                                }
                            else
                                uartPutchar(BEEP);
                            break;
                            // ---------------------------------
                        default:     // We just ignore everything else
                            uartPutchar(BEEP);
                            break;
                    }
                break;

                // --- The delete key has been pressed - do the prettyprinting
            case UART_DELCODE:     // Delete - just handle the printing, buffer manipulation was already done
                uartPutsn(BACKDEL);
                break;
                // ---------------------------------
            default:
                break;
        }
}
// ============================================================================================
void commandUartUnlocked(void)

// When the UART has become unlocked (i.e. can receive characters) then print a command prompt

{
    printf("%s" ERASEEOL, profileState());
}
// ============================================================================================
void commandRefreshPrompt(void)

// Refresh the prompt because something has changed

{
    // Kill the periodic refresh if we've been called for any other reason
    if (timerRunning(&t)) timerDel(&t);

    timerAdd(&t, TIMER_ORIGIN_COMMAND, 0, CL_REFRESH_INTERVAL);
    if (uartIsLocked()) return;
    printf(CURSOROFF "\r%s%s" ERASEEOL CURSORON, profileState(), uartGets());
}
// ============================================================================================
void commandInit(void)

// The wakeup and initialise code for the command module

{
    _doConnected();
    paramsModified = FALSE;
    timerInit(&t);
    timerAdd(&t, TIMER_ORIGIN_COMMAND, 0, CL_REFRESH_INTERVAL);
}
// ============================================================================================
