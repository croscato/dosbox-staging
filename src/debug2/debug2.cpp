/*
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"
#include "programs.h"
#include "setup.h"
#include "string_utils.h"
#include "shell.h"
#include "regs.h"
#include "video.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"

#include <SDL2/SDL.h>
#include <SDL_opengl.h>
#include <fmt/core.h>

#define UNUSED(param) ((void)(param))

extern std::atomic<int> CPU_Cycles;
extern std::atomic<int> CPU_CycleLeft;

class DEBUG2 final : public Program {
public:
    void Run(void) override;
};

const uint32_t kWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

Bitu debug2_callback;

namespace {
    struct Color { float r, g, b, a; };

    struct ColorScheme {
        Color background;
    };

    SDL_Window *window{nullptr};
    SDL_GLContext context{nullptr};
    SDL_Window *current_window{nullptr};
    SDL_GLContext current_context{nullptr};

    bool active{false};
    bool debugging{false};

    bool exit_loop = false;

    ColorScheme color_scheme {
        .background{0.110f, 0.110f, 0.110f, 1.0f},
    };
}

void
DEBUG2::Run()
{
    if(cmd->FindExist("/NOMOUSE",false)) {
        real_writed(0, 0x33 << 2, 0);

        return;
    }

    uint16_t command_number = 1;

    if (!cmd->FindCommand(command_number++, temp_line)) {
        return;
    }

    // Get filename
    char filename[128];
    safe_strcpy(filename, temp_line.c_str());

    // Setup commandline
    char args[256+1];
    args[0] = 0;

    bool found = cmd->FindCommand(command_number++,temp_line);

    while (found) {
        if (safe_strlen(args)+temp_line.length()+1>256) {
            break;
        }

        strcat(args,temp_line.c_str());

        found = cmd->FindCommand(command_number++,temp_line);

        if (found) {
            strcat(args," ");
        }
    }

    // Start new shell and execute prog
    active = true;

    // Save cpu state....
    uint16_t oldcs  = SegValue(cs);
    uint32_t oldeip = reg_eip;
    uint16_t oldss  = SegValue(ss);
    uint32_t oldesp = reg_esp;

    current_window = SDL_GL_GetCurrentWindow();
    current_context = SDL_GL_GetCurrentContext();

    window = SDL_CreateWindow(
        "Debug",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        kWindowFlags
    );

    context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, context);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_GL_SwapWindow(window);

    SDL_GL_MakeCurrent(current_window, current_context);

    // Start shell
    DOS_Shell shell;

    if (!shell.ExecuteProgram(filename, args)) {
        WriteOut(MSG_Get("PROGRAM_EXECUTABLE_MISSING"), filename);
    }

    // set old reg values
    SegSet16(ss,oldss);
    reg_esp = oldesp;

    SegSet16(cs,oldcs);
    reg_eip = oldeip;
}

void
DEBUG2_ShutDown(Section *section)
{
    UNUSED(section);

    if (context) {
        SDL_GL_DeleteContext(context);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }
}

uint32_t
DEBUG2_CheckKeys(void)
{
    return 0;
}

Bitu
DEBUG2_Loop(void)
{
    GFX_Events();

    // Interrupt started ? - then skip it
    uint16_t old_cs  = SegValue(cs);
    uint32_t old_eip = reg_eip;

    PIC_runIRQs();

    Delay(1);

    if ((old_cs != SegValue(cs)) || (old_eip != reg_eip)) {
        //CBreakpoint::AddBreakpoint(oldCS, oldEIP, true);
        //CBreakpoint::ActivateBreakpointsExceptAt(SegPhys(cs) + reg_eip);

        debugging = false;

        DOSBOX_SetNormalLoop();

        return 0;
    }

    return DEBUG2_CheckKeys();
}

void
DEBUG2_DrawScreen(void)
{
    SDL_GL_MakeCurrent(window, context);

    glClearColor(
        color_scheme.background.r, color_scheme.background.g,
        color_scheme.background.b, color_scheme.background.a
    );

    glClear(GL_COLOR_BUFFER_BIT);

    SDL_GL_SwapWindow(window);

    SDL_GL_MakeCurrent(current_window, current_context);
}

void
DEBUG2_Enable(bool pressed)
{
    if (!pressed) {
        return;
    }

    //// Maybe construct the debugger's UI
    //static bool was_ui_started = false;
    //if (!was_ui_started) {
    //DBGUI_StartUp();
    //was_ui_started = (pdc_window != nullptr);
    //}
    //
    //// The debugger is run in release mode so cannot use asserts
    //if (!was_ui_started) {
    //LOG_ERR("DEBUG: Failed to start up the debug window");
    //return;
    //}

    // Defocus the graphical UI and bring the debugger UI into focus
    GFX_LosingFocus();

    SDL_RaiseWindow(window);
    SDL_SetWindowInputFocus(window);

    //SetCodeWinStart();

    DEBUG2_DrawScreen();

    // Maybe show help for the first time in the debugger
    //static bool was_help_shown = false;
    //
    //if (!was_help_shown) {
    //    DEBUG_ShowMsg("***| TYPE HELP (+ENTER) TO GET AN OVERVIEW OF ALL COMMANDS |***\n");
    //
    //    was_help_shown = true;
    //}

    // Start the debugging loops
    debugging = true;
    DOSBOX_SetLoop(&DEBUG2_Loop);

    KEYBOARD_ClrBuffer();
}

Bitu DEBUG2_EnableDebugger()
{
    exit_loop = true;

    DEBUG2_Enable(true);

    CPU_Cycles = CPU_CycleLeft = 0;

    return 0;
}

void
DEBUG2_Init(Section* section)
{
    PROGRAMS_MakeFile("DBG.COM", ProgramCreate<DEBUG2>);

    debug2_callback = CALLBACK_Allocate();

    CALLBACK_Setup(
        static_cast<callback_number_t>(debug2_callback), DEBUG2_EnableDebugger, CB_RETF, "debugger"
    );

    section->AddDestroyFunction(&DEBUG2_ShutDown);
}

int32_t
DEBUG2_Run(int32_t amount,bool quickexit)
{
    UNUSED(amount);
    UNUSED(quickexit);

    return 0;
}

bool
DEBUG2_HeavyIsBreakpoint(void) {
    return active;

    //static Bitu zero_count = 0;
    //if (cpuLog) {
    //    if (cpuLogCounter>0) {
    //        LogInstruction(SegValue(cs),reg_eip,cpuLogFile);
    //        cpuLogCounter--;
    //    }
    //    if (cpuLogCounter<=0) {
    //        cpuLogFile.flush();
    //        cpuLogFile.close();
    //        DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT created\n");
    //        cpuLog = false;
    //        DEBUG_EnableDebugger();
    //        return true;
    //    }
    //}
    //// LogInstruction
    //if (logHeavy) DEBUG_HeavyLogInstruction();
    //if (zeroProtect) {
    //    uint32_t value = 0;
    //    if (!mem_readd_checked(SegPhys(cs)+reg_eip,&value)) {
    //        if (value == 0) zero_count++;
    //        else zero_count = 0;
    //    }
    //    if (zero_count == 10) {
    //        E_Exit("running zeroed code");
    //    }
    //}
    //
    //if (skipFirstInstruction) {
    //    skipFirstInstruction = false;
    //    return false;
    //}
    //if (BPoints.size() && CBreakpoint::CheckBreakpoint(SegValue(cs), reg_eip))
    //    return true;
    //
    //return false;
}
