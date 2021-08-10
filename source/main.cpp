#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "debugger.hpp"
#include "dmntcht.h"
#include "memory_dump.hpp"
#ifdef CUSTOM
#include "Battery.hpp"
#endif

#define NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD 0x80044715
#define FieldDescriptor uint32_t
PadState pad;
// typedef struct {
//     u32 A = 0, B = 0;                                   ///< UniquePadId
// } Breeze_state;
// Breeze_state Bstate = {};

//Common
Thread t0;
Thread t1;
Thread t2;
Thread t3;
Thread t4;
Thread t5;
Thread t6;
Thread t7;
constinit uint64_t systemtickfrequency = 19200000;
bool threadexit = false;
bool threadexit2 = false;
bool Atmosphere_present = false;
uint64_t refreshrate = 1;
FanController g_ICon;

//Mini mode
char Variables[672];

//Checks
Result clkrstCheck = 1;
Result nvCheck = 1;
Result pcvCheck = 1;
Result tsCheck = 1;
Result fanCheck = 1;
Result tcCheck = 1;
Result Hinted = 1;
Result pmdmntCheck = 1;
Result dmntchtCheck = 1;
#ifdef CUSTOM
Result psmCheck = 1;

//Battery
Service *psmService = 0;
BatteryChargeInfoFields *_batteryChargeInfoFields = 0;
char Battery_c[320];
#endif

//Temperatures
int32_t SoC_temperaturemiliC = 0;
int32_t PCB_temperaturemiliC = 0;
int32_t skin_temperaturemiliC = 0;
char SoCPCB_temperature_c[64];
char skin_temperature_c[32];

//CPU Usage
uint64_t idletick0 = 19200000;
uint64_t idletick1 = 19200000;
uint64_t idletick2 = 19200000;
uint64_t idletick3 = 19200000;
char CPU_Usage0[32];
char CPU_Usage1[32];
char CPU_Usage2[32];
char CPU_Usage3[32];
char CPU_compressed_c[160];

//Frequency
///CPU
uint32_t CPU_Hz = 0;
char CPU_Hz_c[32];
///GPU
uint32_t GPU_Hz = 0;
char GPU_Hz_c[32];
///RAM
uint32_t RAM_Hz = 0;
char RAM_Hz_c[32];

//RAM Size
char RAM_all_c[64];
char RAM_application_c[64];
char RAM_applet_c[64];
char RAM_system_c[64];
char RAM_systemunsafe_c[64];
char RAM_compressed_c[320];
char RAM_var_compressed_c[320];
uint64_t RAM_Total_all_u = 0;
uint64_t RAM_Total_application_u = 0;
uint64_t RAM_Total_applet_u = 0;
uint64_t RAM_Total_system_u = 0;
uint64_t RAM_Total_systemunsafe_u = 0;
uint64_t RAM_Used_all_u = 0;
uint64_t RAM_Used_application_u = 0;
uint64_t RAM_Used_applet_u = 0;
uint64_t RAM_Used_system_u = 0;
uint64_t RAM_Used_systemunsafe_u = 0;

//Fan
float Rotation_SpeedLevel_f = 0;
char Rotation_SpeedLevel_c[64];

//GPU Usage
FieldDescriptor fd = 0;
uint32_t GPU_Load_u = 0;
char GPU_Load_c[32];

//NX-FPS
bool GameRunning = false;
bool check = false;
bool SaltySD = false;
uintptr_t FPSaddress = 0x0;
uintptr_t FPSavgaddress = 0x0;
uint64_t PID = 0;
uint8_t FPS = 0xFE;
float FPSavg = 254;
char FPS_c[32];
char FPSavg_c[32];
char FPS_compressed_c[64];
char FPS_var_compressed_c[64];
Handle debug;

//Check if SaltyNX is working
bool CheckPort() {
    Handle saltysd;
    for (int i = 0; i < 67; i++) {
        if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
            svcCloseHandle(saltysd);
            break;
        } else {
            if (i == 66) return false;
            svcSleepThread(1'000'000);
        }
    }
    for (int i = 0; i < 67; i++) {
        if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
            svcCloseHandle(saltysd);
            return true;
        } else
            svcSleepThread(1'000'000);
    }
    return false;
}

bool isServiceRunning(const char *serviceName) {
    Handle handle;
    SmServiceName service_name = smEncodeName(serviceName);
    if (R_FAILED(smRegisterService(&handle, service_name, false, 1)))
        return true;
    else {
        svcCloseHandle(handle);
        smUnregisterService(service_name);
        return false;
    }
}

void CheckIfGameRunning(void *) {
    while (threadexit2 == false) {
        if (R_FAILED(pmdmntGetApplicationProcessId(&PID))) {
            if (check == false) {
                remove("sdmc:/SaltySD/FPSoffset.hex");
                check = true;
            }
            GameRunning = false;
            svcCloseHandle(debug);
        } else if (GameRunning == false) {
            svcSleepThread(1'000'000'000);
            FILE *FPSoffset = fopen("sdmc:/SaltySD/FPSoffset.hex", "rb");
            if (FPSoffset != NULL) {
                if (Atmosphere_present == true) {
                    bool out = false;
                    dmntchtHasCheatProcess(&out);
                    if (out == false) dmntchtForceOpenCheatProcess();
                } else
                    svcSleepThread(1'000'000'000);
                fread(&FPSaddress, 0x5, 1, FPSoffset);
                FPSavgaddress = FPSaddress - 0x8;
                fclose(FPSoffset);
                GameRunning = true;
                check = false;
            }
        }
        svcSleepThread(1'000'000'000);
    }
}

//Check for input outside of FPS limitations
void CheckButtons(void *) {
    padInitializeAny(&pad);
    static uint64_t kHeld = padGetButtons(&pad);  // hidKeysHeld(CONTROLLER_P1_AUTO);
    while (threadexit == false) {
        padUpdate(&pad);
        kHeld = padGetButtons(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if ((kHeld & HidNpadButton_ZR) && (kHeld & HidNpadButton_R)) {
            if (kHeld & HidNpadButton_Down) {
                TeslaFPS = 1;
                refreshrate = 1;
                systemtickfrequency = 19200000;
            } else if (kHeld & HidNpadButton_Up) {
                TeslaFPS = 5;
                refreshrate = 5;
                systemtickfrequency = 3840000;
            }
        }
        if ((kHeld & HidNpadButton_ZR) && (kDown & HidNpadButton_A)) {
            Bstate.A += 2;
        };
        if ((kHeld & HidNpadButton_ZR) && (kDown & HidNpadButton_B)) {
            Bstate.B += 2;
        };

        svcSleepThread(100'000'000);
    }
}

//Stuff that doesn't need multithreading
void Misc(void *) {
    while (threadexit == false) {
        // CPU, GPU and RAM Frequency
        if (R_SUCCEEDED(clkrstCheck)) {
            ClkrstSession clkSession;
            if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_CpuBus, 3))) {
                clkrstGetClockRate(&clkSession, &CPU_Hz);
                clkrstCloseSession(&clkSession);
            }
            if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_GPU, 3))) {
                clkrstGetClockRate(&clkSession, &GPU_Hz);
                clkrstCloseSession(&clkSession);
            }
            if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_EMC, 3))) {
                clkrstGetClockRate(&clkSession, &RAM_Hz);
                clkrstCloseSession(&clkSession);
            }
        } else if (R_SUCCEEDED(pcvCheck)) {
            pcvGetClockRate(PcvModule_CpuBus, &CPU_Hz);
            pcvGetClockRate(PcvModule_GPU, &GPU_Hz);
            pcvGetClockRate(PcvModule_EMC, &RAM_Hz);
        }

        //Temperatures
        if (R_SUCCEEDED(tsCheck)) {
            tsGetTemperatureMilliC(TsLocation_External, &SoC_temperaturemiliC);
            tsGetTemperatureMilliC(TsLocation_Internal, &PCB_temperaturemiliC);
        }
        if (R_SUCCEEDED(tcCheck)) tcGetSkinTemperatureMilliC(&skin_temperaturemiliC);

        //RAM Memory Used
        if (R_SUCCEEDED(Hinted)) {
            svcGetSystemInfo(&RAM_Total_application_u, 0, INVALID_HANDLE, 0);
            svcGetSystemInfo(&RAM_Total_applet_u, 0, INVALID_HANDLE, 1);
            svcGetSystemInfo(&RAM_Total_system_u, 0, INVALID_HANDLE, 2);
            svcGetSystemInfo(&RAM_Total_systemunsafe_u, 0, INVALID_HANDLE, 3);
            svcGetSystemInfo(&RAM_Used_application_u, 1, INVALID_HANDLE, 0);
            svcGetSystemInfo(&RAM_Used_applet_u, 1, INVALID_HANDLE, 1);
            svcGetSystemInfo(&RAM_Used_system_u, 1, INVALID_HANDLE, 2);
            svcGetSystemInfo(&RAM_Used_systemunsafe_u, 1, INVALID_HANDLE, 3);
        }

        //Fan
        if (R_SUCCEEDED(fanCheck)) fanControllerGetRotationSpeedLevel(&g_ICon, &Rotation_SpeedLevel_f);

        //GPU Load
        if (R_SUCCEEDED(nvCheck)) nvIoctl(fd, NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD, &GPU_Load_u);

        //FPS
        if (GameRunning == true) {
            if (Atmosphere_present == true) {
                dmntchtReadCheatProcessMemory(FPSaddress, &FPS, 0x1);
                dmntchtReadCheatProcessMemory(FPSavgaddress, &FPSavg, 0x4);
            } else if (R_SUCCEEDED(svcDebugActiveProcess(&debug, PID))) {
                svcReadDebugProcessMemory(&FPS, debug, FPSaddress, 0x1);
                svcReadDebugProcessMemory(&FPSavg, debug, FPSavgaddress, 0x4);
                svcCloseHandle(debug);
            }
        }

        // Interval
        svcSleepThread(1'000'000'000 / refreshrate);
    }
}

//Check each core for idled ticks in intervals, they cannot read info about other core than they are assigned
void CheckCore0(void *) {
    while (threadexit == false) {
        static uint64_t idletick_a0 = 0;
        static uint64_t idletick_b0 = 0;
        svcGetInfo(&idletick_b0, InfoType_IdleTickCount, INVALID_HANDLE, 0);
        svcSleepThread(1'000'000'000 / refreshrate);
        svcGetInfo(&idletick_a0, InfoType_IdleTickCount, INVALID_HANDLE, 0);
        idletick0 = idletick_a0 - idletick_b0;
    }
}

void CheckCore1(void *) {
    while (threadexit == false) {
        static uint64_t idletick_a1 = 0;
        static uint64_t idletick_b1 = 0;
        svcGetInfo(&idletick_b1, InfoType_IdleTickCount, INVALID_HANDLE, 1);
        svcSleepThread(1'000'000'000 / refreshrate);
        svcGetInfo(&idletick_a1, InfoType_IdleTickCount, INVALID_HANDLE, 1);
        idletick1 = idletick_a1 - idletick_b1;
    }
}

void CheckCore2(void *) {
    while (threadexit == false) {
        static uint64_t idletick_a2 = 0;
        static uint64_t idletick_b2 = 0;
        svcGetInfo(&idletick_b2, InfoType_IdleTickCount, INVALID_HANDLE, 2);
        svcSleepThread(1'000'000'000 / refreshrate);
        svcGetInfo(&idletick_a2, InfoType_IdleTickCount, INVALID_HANDLE, 2);
        idletick2 = idletick_a2 - idletick_b2;
    }
}

void CheckCore3(void *) {
    while (threadexit == false) {
        static uint64_t idletick_a3 = 0;
        static uint64_t idletick_b3 = 0;
        svcGetInfo(&idletick_b3, InfoType_IdleTickCount, INVALID_HANDLE, 3);
        svcSleepThread(1'000'000'000 / refreshrate);
        svcGetInfo(&idletick_a3, InfoType_IdleTickCount, INVALID_HANDLE, 3);
        idletick3 = idletick_a3 - idletick_b3;
    }
}

//Start reading all stats
void StartThreads() {
    threadCreate(&t0, CheckCore0, NULL, NULL, 0x100, 0x10, 0);
    threadCreate(&t1, CheckCore1, NULL, NULL, 0x100, 0x10, 1);
    threadCreate(&t2, CheckCore2, NULL, NULL, 0x100, 0x10, 2);
    threadCreate(&t3, CheckCore3, NULL, NULL, 0x100, 0x10, 3);
    threadCreate(&t4, Misc, NULL, NULL, 0x100, 0x3F, -2);
    threadCreate(&t5, CheckButtons, NULL, NULL, 0x400, 0x3F, -2);
    threadStart(&t0);
    threadStart(&t1);
    threadStart(&t2);
    threadStart(&t3);
    threadStart(&t4);
    threadStart(&t5);
}

//End reading all stats
void CloseThreads() {
    threadexit = true;
    threadexit2 = true;
    threadWaitForExit(&t0);
    threadWaitForExit(&t1);
    threadWaitForExit(&t2);
    threadWaitForExit(&t3);
    threadWaitForExit(&t4);
    threadWaitForExit(&t5);
    threadWaitForExit(&t6);
    threadWaitForExit(&t7);
    threadClose(&t0);
    threadClose(&t1);
    threadClose(&t2);
    threadClose(&t3);
    threadClose(&t4);
    threadClose(&t5);
    threadClose(&t6);
    threadClose(&t7);
    threadexit = false;
    threadexit2 = false;
}

//Separate functions dedicated to "FPS Counter" mode
void FPSCounter(void *) {
    while (threadexit == false) {
        if (GameRunning == true) {
            if (Atmosphere_present == true)
                dmntchtReadCheatProcessMemory(FPSavgaddress, &FPSavg, 0x4);
            else if (R_SUCCEEDED(svcDebugActiveProcess(&debug, PID))) {
                svcReadDebugProcessMemory(&FPSavg, debug, FPSavgaddress, 0x4);
                svcCloseHandle(debug);
            }
        } else
            FPSavg = 254;
        //interval
        svcSleepThread(1'000'000'000 / refreshrate);
    }
}

void StartFPSCounterThread() {
    threadCreate(&t0, FPSCounter, NULL, NULL, 0x1000, 0x3F, 3);
    threadStart(&t0);
}

void EndFPSCounterThread() {
    threadexit = true;
    threadWaitForExit(&t0);
    threadClose(&t0);
    threadexit = false;
}

//FPS Counter mode
class com_FPS : public tsl::Gui {
   public:
    com_FPS() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            static uint8_t avg = 0;
            if (FPSavg < 10) avg = 0;
            if (FPSavg >= 10) avg = 23;
            if (FPSavg >= 100) avg = 46;
            renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 370 + avg, 50, a(0x7111));
            renderer->drawString(FPSavg_c, false, 5, 40, 40, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        ///FPS
        snprintf(FPSavg_c, sizeof FPSavg_c, "%2.1f", FPSavg);
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            EndFPSCounterThread();
            tsl::goBack();
            return true;
        }
        return false;
    }
};

//Full mode
class FullOverlay : public tsl::Gui {
   public:
    FullOverlay() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("Full mode", APP_VERSION);

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            //Print strings
            ///CPU
            if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) {
                renderer->drawString("CPU Usage:", false, 20, 120, 20, renderer->a(0xFFFF));
                renderer->drawString(CPU_Hz_c, false, 20, 155, 15, renderer->a(0xFFFF));
                renderer->drawString(CPU_compressed_c, false, 20, 185, 15, renderer->a(0xFFFF));
            }

            ///GPU
            if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(nvCheck)) {
                renderer->drawString("GPU Usage:", false, 20, 285, 20, renderer->a(0xFFFF));
                if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(GPU_Hz_c, false, 20, 320, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(nvCheck)) renderer->drawString(GPU_Load_c, false, 20, 335, 15, renderer->a(0xFFFF));
            }

            ///RAM
            if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(Hinted)) {
                renderer->drawString("RAM Usage:", false, 20, 375, 20, renderer->a(0xFFFF));
                if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(RAM_Hz_c, false, 20, 410, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(Hinted)) {
                    renderer->drawString(RAM_compressed_c, false, 20, 440, 15, renderer->a(0xFFFF));
                    renderer->drawString(RAM_var_compressed_c, false, 140, 440, 15, renderer->a(0xFFFF));
                }
            }

            ///Thermal
            if (R_SUCCEEDED(tsCheck) || R_SUCCEEDED(tcCheck) || R_SUCCEEDED(fanCheck)) {
                renderer->drawString("Thermal:", false, 20, 540, 20, renderer->a(0xFFFF));
                if (R_SUCCEEDED(tsCheck)) renderer->drawString(SoCPCB_temperature_c, false, 20, 575, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(tcCheck)) renderer->drawString(skin_temperature_c, false, 20, 605, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(fanCheck)) renderer->drawString(Rotation_SpeedLevel_c, false, 20, 620, 15, renderer->a(0xFFFF));
            }

            ///FPS
            if (GameRunning == true) {
                renderer->drawString(FPS_compressed_c, false, 235, 120, 20, renderer->a(0xFFFF));
                renderer->drawString(FPS_var_compressed_c, false, 295, 120, 20, renderer->a(0xFFFF));
            }

            if (refreshrate == 5)
                renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Down to slow down refresh", false, 20, 675, 15, renderer->a(0xFFFF));
            else if (refreshrate == 1)
                renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Up to speed up refresh", false, 20, 675, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS == 60) TeslaFPS = 1;
        //In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
        //This is because making each loop also takes time, which is not considered because this will take also additional time
        if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
        if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
        if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
        if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;

        //Make stuff ready to print
        ///CPU
        snprintf(CPU_Hz_c, sizeof CPU_Hz_c, "Frequency: %.1f MHz", (float)CPU_Hz / 1000000);
        snprintf(CPU_Usage0, sizeof CPU_Usage0, "Core #0: %.2f%s", ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage1, sizeof CPU_Usage1, "Core #1: %.2f%s", ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage2, sizeof CPU_Usage2, "Core #2: %.2f%s", ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage3, sizeof CPU_Usage3, "Core #3: %.2f%s", ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "%s\n%s\n%s\n%s", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3);

        ///GPU
        snprintf(GPU_Hz_c, sizeof GPU_Hz_c, "Frequency: %.1f MHz", (float)GPU_Hz / 1000000);
        snprintf(GPU_Load_c, sizeof GPU_Load_c, "Load: %.1f%s", (float)GPU_Load_u / 10, "%");

        ///RAM
        snprintf(RAM_Hz_c, sizeof RAM_Hz_c, "Frequency: %.1f MHz", (float)RAM_Hz / 1000000);
        float RAM_Total_application_f = (float)RAM_Total_application_u / 1024 / 1024;
        float RAM_Total_applet_f = (float)RAM_Total_applet_u / 1024 / 1024;
        float RAM_Total_system_f = (float)RAM_Total_system_u / 1024 / 1024;
        float RAM_Total_systemunsafe_f = (float)RAM_Total_systemunsafe_u / 1024 / 1024;
        float RAM_Total_all_f = RAM_Total_application_f + RAM_Total_applet_f + RAM_Total_system_f + RAM_Total_systemunsafe_f;
        float RAM_Used_application_f = (float)RAM_Used_application_u / 1024 / 1024;
        float RAM_Used_applet_f = (float)RAM_Used_applet_u / 1024 / 1024;
        float RAM_Used_system_f = (float)RAM_Used_system_u / 1024 / 1024;
        float RAM_Used_systemunsafe_f = (float)RAM_Used_systemunsafe_u / 1024 / 1024;
        float RAM_Used_all_f = RAM_Used_application_f + RAM_Used_applet_f + RAM_Used_system_f + RAM_Used_systemunsafe_f;
        snprintf(RAM_all_c, sizeof RAM_all_c, "Total:");
        snprintf(RAM_application_c, sizeof RAM_application_c, "Application:");
        snprintf(RAM_applet_c, sizeof RAM_applet_c, "Applet:");
        snprintf(RAM_system_c, sizeof RAM_system_c, "System:");
        snprintf(RAM_systemunsafe_c, sizeof RAM_systemunsafe_c, "System Unsafe:");
        snprintf(RAM_compressed_c, sizeof RAM_compressed_c, "%s\n%s\n%s\n%s\n%s", RAM_all_c, RAM_application_c, RAM_applet_c, RAM_system_c, RAM_systemunsafe_c);
        snprintf(RAM_all_c, sizeof RAM_all_c, "%4.2f / %4.2f MB", RAM_Used_all_f, RAM_Total_all_f);
        snprintf(RAM_application_c, sizeof RAM_application_c, "%4.2f / %4.2f MB", RAM_Used_application_f, RAM_Total_application_f);
        snprintf(RAM_applet_c, sizeof RAM_applet_c, "%4.2f / %4.2f MB", RAM_Used_applet_f, RAM_Total_applet_f);
        snprintf(RAM_system_c, sizeof RAM_system_c, "%4.2f / %4.2f MB", RAM_Used_system_f, RAM_Total_system_f);
        snprintf(RAM_systemunsafe_c, sizeof RAM_systemunsafe_c, "%4.2f / %4.2f MB", RAM_Used_systemunsafe_f, RAM_Total_systemunsafe_f);
        snprintf(RAM_var_compressed_c, sizeof RAM_var_compressed_c, "%s\n%s\n%s\n%s\n%s", RAM_all_c, RAM_application_c, RAM_applet_c, RAM_system_c, RAM_systemunsafe_c);

        ///Thermal
        snprintf(SoCPCB_temperature_c, sizeof SoCPCB_temperature_c, "SoC: %2.2f \u00B0C\nPCB: %2.2f \u00B0C", (float)SoC_temperaturemiliC / 1000, (float)PCB_temperaturemiliC / 1000);
        snprintf(skin_temperature_c, sizeof skin_temperature_c, "Skin: %2.2f \u00B0C", (float)skin_temperaturemiliC / 1000);
        snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "Fan: %2.2f%s", Rotation_SpeedLevel_f * 100, "%");

        ///FPS
        snprintf(FPS_c, sizeof FPS_c, "PFPS:");       //Pushed Frames Per Second
        snprintf(FPSavg_c, sizeof FPSavg_c, "FPS:");  //Frames Per Second calculated from averaged frametime
        snprintf(FPS_compressed_c, sizeof FPS_compressed_c, "%s\n%s", FPS_c, FPSavg_c);
        snprintf(FPS_var_compressed_c, sizeof FPS_var_compressed_c, "%u\n%2.2f", FPS, FPSavg);
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            CloseThreads();
            tsl::goBack();
            return true;
        }
        return false;
    }
};
// Bookmark display
// WIP
#define MAX_POINTER_DEPTH 12
struct pointer_chain_t {
    u64 depth = 0;
    s64 offset[MAX_POINTER_DEPTH + 1] = {0};  // offset to address pointed by pointer
};
struct bookmark_t {
    char label[19] = {0};
    searchType_t type;
    pointer_chain_t pointer;
    bool heap = true;
    u64 offset = 0;
    bool deleted = false;
    u8 multiplier = 0;
	u16 magic = 0x1289;
};
#define NUM_bookmark 10
#define NUM_cheats 20
u32 total_opcode = 0;
#define MaxCheatCount 0x80
#define MaxOpcodes 0x100 // uint32_t opcodes[0x100]
#define MaximumProgramOpcodeCount 0x400
static const std::vector<u32> buttonCodes = {0x80000001,
                                             0x80000002,
                                             0x80000004,
                                             0x80000008,
                                             0x80000010,
                                             0x80000020,
                                             0x80000040,
                                             0x80000080,
                                             0x80000100,
                                             0x80000200,
                                             0x80000400,
                                             0x80000800,
                                             0x80001000,
                                             0x80002000,
                                             0x80004000,
                                             0x80008000,
                                             0x80010000,
                                             0x80020000,
                                             0x80040000,
                                             0x80080000,
                                             0x80100000,
                                             0x80200000,
                                             0x80400000,
                                             0x80800000};
static const std::vector<std::string> buttonNames = {"\uE0A0 ", "\uE0A1 ", "\uE0A2 ", "\uE0A3 ", "\uE0C4 ", "\uE0C5 ", "\uE0A4 ", "\uE0A5 ", "\uE0A6 ", "\uE0A7 ", "\uE0B3 ", "\uE0B4 ", "\uE0B1 ", "\uE0AF ", "\uE0B2 ", "\uE0B0 ", "\uE091 ", "\uE092 ", "\uE090 ", "\uE093 ", "\uE145 ", "\uE143 ", "\uE146 ", "\uE144 "};
char BookmarkLabels[NUM_bookmark * 20 + NUM_cheats * 0x41] = "";
char Cursor[NUM_bookmark * 5 + NUM_cheats * 5] = "";
char MultiplierStr[NUM_bookmark * 5 + NUM_cheats * 5] = "";
char CheatsLabelsStr[NUM_cheats * 0x41] = "";
char CheatsCursor[NUM_cheats * 5] = "";
char CheatsEnableStr[NUM_cheats * 5] = "";
bool m_show_only_enabled_cheats = true;
bool m_cursor_on_bookmark = true;
bool m_no_cheats = true;
bool m_no_bookmarks = true;
bool m_game_not_running = true;
u8 m_displayed_bookmark_lines = 0;
u8 m_displayed_cheat_lines = 0;
u8 m_index = 0;
u8 m_cheat_index = 0;
std::string m_edizon_dir = "/switch/EdiZon";
std::string m_store_extension = "A";
Debugger *m_debugger;
// MemoryDump *m_memoryDump;
MemoryDump *m_AttributeDumpBookmark;
u8 m_addresslist_offset = 0;
u8 m_cheatlist_offset = 0;
bool m_32bitmode = false;
static const std::vector<u8> dataTypeSizes = {1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 8};
searchValue_t m_oldvalue[NUM_bookmark] = {0};
DmntCheatProcessMetadata metadata;
// char Variables[NUM_bookmark*20];
char bookmarkfilename[200] = "bookmark filename";
bool init_se_tools() {
    if (dmntchtCheck == 1) dmntchtCheck = dmntchtInitialize();

    m_debugger = new Debugger();
    dmntchtForceOpenCheatProcess();
    dmntchtHasCheatProcess(&(m_debugger->m_dmnt));
    if (m_debugger->m_dmnt)
        dmntchtGetCheatProcessMetadata(&metadata);
    else {
        return false;
        LoaderModuleInfo proc_modules[2] = {};
        s32 num_modules = 2;
        ldrDmntInitialize();
        Result rc = ldrDmntGetProcessModuleInfo(m_debugger->getRunningApplicationPID(), proc_modules, std::size(proc_modules), &num_modules);
        ldrDmntExit();
        const LoaderModuleInfo *proc_module = nullptr;
        if (num_modules == 2) {
            proc_module = &proc_modules[1];
        } else if (num_modules == 1) {
            proc_module = &proc_modules[0];
        }
        if (rc != 0)
            printf("num_modules = %x, proc_module->base_address = %lx , pid = %ld, rc = %x\n ", num_modules, proc_module->base_address, m_debugger->getRunningApplicationPID(), rc);
        metadata.main_nso_extents.base = proc_module->base_address;
        metadata.main_nso_extents.size = proc_module->size;
        Handle proc_h;        
        {
            typedef struct {
                u64 keys_held;
                u64 flags;
            } CfgOverrideStatus;
            struct {
                NcmProgramLocation loc;
                CfgOverrideStatus status;
            } out;
            u64 pid = m_debugger->getRunningApplicationPID();
            serviceDispatchInOut(pmdmntGetServiceSession(), 65000, pid, out,
                                             .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
                                             .out_handles = &proc_h, );
        };
        rc = svcGetInfo(&metadata.heap_extents.base, InfoType_HeapRegionAddress, proc_h, 0);
        // printf("metadata.heap_extents.base = %lx rc = %x\n", metadata.heap_extents.base, rc);
        std::memcpy(metadata.main_nso_build_id, proc_module->build_id, sizeof((metadata.main_nso_build_id)));
    };
    // dmntchtGetCheatProcessMetadata(&metadata);
    u8 build_id[0x20];
    memcpy(build_id, metadata.main_nso_build_id, 0x20);

    // if (!(m_debugger->m_dmnt)) {m_debugger->attachToProcess();m_debugger->resume();}
    // check and set m_32bitmode

    snprintf(bookmarkfilename, 200, "%s/%02X%02X%02X%02X%02X%02X%02X%02X.dat", EDIZON_DIR,
             build_id[0], build_id[1], build_id[2], build_id[3], build_id[4], build_id[5], build_id[6], build_id[7]);

    m_AttributeDumpBookmark = new MemoryDump(bookmarkfilename, DumpType::ADDR, false);
    // m_memoryDump = new MemoryDump(EDIZON_DIR "/memdumpbookmark.dat", DumpType::ADDR, false);
    return true;
};
void cleanup_se_tools() {
    // if (dmntchtCheck == 0) dmntchtExit();
    delete m_debugger;
    delete m_AttributeDumpBookmark;
    // delete m_memoryDump;
    return;
};
static std::string _getAddressDisplayString(u64 address, Debugger *debugger, searchType_t searchType) {
    char ss[200];

    searchValue_t searchValue;
    searchValue._u64 = debugger->peekMemory(address);
    // start mod for address content display
    //   u16 k = searchValue._u8;
    //   if (m_searchValueFormat == FORMAT_HEX)
    //   {
    //     switch (dataTypeSizes[searchType])
    //     {
    //     case 1:
    //       ss << "0x" << std::uppercase << std::hex << k;
    //       break;
    //     case 2:
    //       ss << "0x" << std::uppercase << std::hex << searchValue._u16;
    //       break;
    //     default:
    //     case 4:
    //       ss << "0x" << std::uppercase << std::hex << searchValue._u32;
    //       break;
    //     case 8:
    //       ss << "0x" << std::uppercase << std::hex << searchValue._u64;
    //       break;
    //     }
    //   }
    //   else
    {
        // end mod
        switch (searchType) {
            case SEARCH_TYPE_UNSIGNED_8BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._u8);
                // ss << std::dec << static_cast<u64>(searchValue._u8);
                break;
            case SEARCH_TYPE_UNSIGNED_16BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._u16);
                //   ss << std::dec << static_cast<u64>(searchValue._u16);
                break;
            case SEARCH_TYPE_UNSIGNED_32BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._u32);
                //   ss << std::dec << static_cast<u64>(searchValue._u32);
                break;
            case SEARCH_TYPE_UNSIGNED_64BIT:
                snprintf(ss, sizeof ss, "%ld", searchValue._u64);
                //   ss << std::dec << static_cast<u64>(searchValue._u64);
                break;
            case SEARCH_TYPE_SIGNED_8BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._s8);
                //   ss << std::dec << static_cast<s64>(searchValue._s8);
                break;
            case SEARCH_TYPE_SIGNED_16BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._s16);
                //   ss << std::dec << static_cast<s64>(searchValue._s16);
                break;
            case SEARCH_TYPE_SIGNED_32BIT:
                snprintf(ss, sizeof ss, "%d", searchValue._s32);
                //   ss << std::dec << static_cast<s64>(searchValue._s32);
                break;
            case SEARCH_TYPE_SIGNED_64BIT:
                snprintf(ss, sizeof ss, "%ld", searchValue._s64);
                //   ss << std::dec << static_cast<s64>(searchValue._s64);
                break;
            case SEARCH_TYPE_FLOAT_32BIT:
                snprintf(ss, sizeof ss, "%f", searchValue._f32);
                //   ss << std::dec << searchValue._f32;
                break;
            case SEARCH_TYPE_FLOAT_64BIT:
                snprintf(ss, sizeof ss, "%lf", searchValue._f64);
                //   ss << std::dec << searchValue._f64;
                break;
            case SEARCH_TYPE_POINTER:
                snprintf(ss, sizeof ss, "0x%016lX", searchValue._u64);
                //   ss << std::dec << searchValue._u64;
                break;
            case SEARCH_TYPE_NONE:
                break;
        }
    }

    return ss;  //.str();
}
class MailBoxOverlay : public tsl::Gui {
   public:
    MailBoxOverlay() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            if (GameRunning == false)
                renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 80, a(0x7111));
            else
                renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 110, a(0x7111));

            //Print strings
            ///CPU
            if (GameRunning == true)
                renderer->drawString("CPU\nGPU\nRAM\nTEMP\nFAN\nPFPS\nFPS", false, 0, 15, 15, renderer->a(0xFFFF));
            else
                renderer->drawString("A\nB\nTeslaFPS\nTEMP\nFAN", false, 0, 15, 15, renderer->a(0xFFFF));

            ///GPU
            renderer->drawString(Variables, false, 60, 15, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS == 60) TeslaFPS = 1;
        //In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
        //This is because making each loop also takes time, which is not considered because this will take also additional time
        if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
        if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
        if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
        if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;

        //Make stuff ready to print
        ///CPU
        double percent = ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage0, sizeof CPU_Usage0, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage1, sizeof CPU_Usage1, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage2, sizeof CPU_Usage2, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage3, sizeof CPU_Usage3, "%.0f%s", percent, "%");
        snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "[%s,%s,%s,%s]@%.1f", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3, (float)CPU_Hz / 1000000);

        ///GPU
        snprintf(GPU_Load_c, sizeof GPU_Load_c, "%.1f%s@%.1f", (float)GPU_Load_u / 10, "%", (float)GPU_Hz / 1000000);

        ///RAM
        float RAM_Total_application_f = (float)RAM_Total_application_u / 1024 / 1024;
        float RAM_Total_applet_f = (float)RAM_Total_applet_u / 1024 / 1024;
        float RAM_Total_system_f = (float)RAM_Total_system_u / 1024 / 1024;
        float RAM_Total_systemunsafe_f = (float)RAM_Total_systemunsafe_u / 1024 / 1024;
        float RAM_Total_all_f = RAM_Total_application_f + RAM_Total_applet_f + RAM_Total_system_f + RAM_Total_systemunsafe_f;
        float RAM_Used_application_f = (float)RAM_Used_application_u / 1024 / 1024;
        float RAM_Used_applet_f = (float)RAM_Used_applet_u / 1024 / 1024;
        float RAM_Used_system_f = (float)RAM_Used_system_u / 1024 / 1024;
        float RAM_Used_systemunsafe_f = (float)RAM_Used_systemunsafe_u / 1024 / 1024;
        float RAM_Used_all_f = RAM_Used_application_f + RAM_Used_applet_f + RAM_Used_system_f + RAM_Used_systemunsafe_f;
        snprintf(RAM_all_c, sizeof RAM_all_c, "%.0f/%.0fMB", RAM_Used_all_f, RAM_Total_all_f);
        snprintf(RAM_var_compressed_c, sizeof RAM_var_compressed_c, "%s@%.1f", RAM_all_c, (float)RAM_Hz / 1000000);

        ///Thermal
        snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2.1f\u00B0C/%2.1f\u00B0C/%2.1f\u00B0C", (float)SoC_temperaturemiliC / 1000, (float)PCB_temperaturemiliC / 1000, (float)skin_temperaturemiliC / 1000);
        snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "%2.2f%s", Rotation_SpeedLevel_f * 100, "%");

        ///FPS
        snprintf(FPS_c, sizeof FPS_c, "PFPS:");       //Pushed Frames Per Second
        snprintf(FPSavg_c, sizeof FPSavg_c, "FPS:");  //Frames Per Second calculated from averaged frametime
        snprintf(FPS_compressed_c, sizeof FPS_compressed_c, "%s\n%s", FPS_c, FPSavg_c);
        snprintf(FPS_var_compressed_c, sizeof FPS_compressed_c, "%u\n%2.2f", FPS, FPSavg);

        if (GameRunning == true)
            snprintf(Variables, sizeof Variables, "%s\n%s\n%s\n%s\n%s\n%s", CPU_compressed_c, GPU_Load_c, RAM_var_compressed_c, skin_temperature_c, Rotation_SpeedLevel_c, FPS_var_compressed_c);
        else
            snprintf(Variables, sizeof Variables, "%d\n%d\n%d\n%s\n%s", Bstate.A, Bstate.B, TeslaFPS, skin_temperature_c, Rotation_SpeedLevel_c);
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            CloseThreads();
            tsl::goBack();
            return true;
        };
        if (keysDown & HidNpadButton_B && keysHeld & HidNpadButton_ZL) {
            CloseThreads();
            tsl::goBack();
            return true;
        }
        return false;
    }
};
u64 m_cheatCnt = 0; 
DmntCheatEntry *m_cheats = nullptr;
bool refresh_cheats = true;

void getcheats(){ // WIP
    char ss[200] = "";
    if (refresh_cheats) {
        if (m_cheatCnt != 0) delete m_cheats;
        dmntchtGetCheatCount(&m_cheatCnt);
        if (m_cheatCnt > 0) {
            m_cheats = new DmntCheatEntry[m_cheatCnt];
            dmntchtGetCheats(m_cheats, m_cheatCnt, 0, &m_cheatCnt);
        } else {
            snprintf(CheatsEnableStr, sizeof CheatsEnableStr, "No Cheats available\n");
            return;
        }
        refresh_cheats = false;
    };
    if (m_show_only_enabled_cheats) {
        snprintf(CheatsLabelsStr, sizeof CheatsLabelsStr, "\n");
        snprintf(CheatsCursor, sizeof CheatsCursor, "\n");
        snprintf(CheatsEnableStr, sizeof CheatsEnableStr, "Enabled Cheats\n");
        m_displayed_cheat_lines = 0;
    } else {
        total_opcode = 0;
        for (u8 i = 0; i < m_cheatCnt; i++)
            if (m_cheats[i].enabled) total_opcode += m_cheats[i].definition.num_opcodes;
        snprintf(CheatsCursor, sizeof CheatsCursor, "Cheats %d/%ld opcode = %d Total opcode = %d/%d\n", m_cheat_index + m_cheatlist_offset + 1, m_cheatCnt, m_cheats[m_cheat_index + m_cheatlist_offset].definition.num_opcodes, total_opcode, MaximumProgramOpcodeCount);
        snprintf(CheatsLabelsStr, sizeof CheatsLabelsStr, "\n");
        snprintf(CheatsEnableStr, sizeof CheatsEnableStr, "\n");
    };

    for (u8 line = 0; line < NUM_cheats; line++) {
        while (m_show_only_enabled_cheats && !(m_cheats[line + m_cheatlist_offset].enabled)){
             m_cheatlist_offset ++;
             if ((line + m_cheatlist_offset) >= m_cheatCnt)
                 break;
        };
        if ((line + m_cheatlist_offset) >= m_cheatCnt)
            break;
        {   
            char namestr[100] = "";         
            int buttoncode = m_cheats[line + m_cheatlist_offset].definition.opcodes[0];
            if ((buttoncode & 0xF0000000) == 0x80000000)
                for (u32 i = 0; i < buttonCodes.size(); i++) {
                    if ((buttoncode & buttonCodes[i]) == buttonCodes[i])
                        strcat(namestr, buttonNames[i].c_str());
                }
            snprintf(ss, sizeof ss, "%s%s\n", namestr, m_cheats[line + m_cheatlist_offset].definition.readable_name);
            strcat(CheatsLabelsStr, ss);
            m_displayed_cheat_lines++;
            snprintf(ss, sizeof ss, "%s\n", ((m_cheat_index == line) && (!m_show_only_enabled_cheats) && !m_cursor_on_bookmark) ? "\uE019" : "");
            strcat(CheatsCursor, ss);
            if (m_show_only_enabled_cheats)
                snprintf(ss, sizeof ss, "\n");
            else
                snprintf(ss, sizeof ss, "%s\n", (m_cheats[line + m_cheatlist_offset].enabled) ? "\u25A0" : "\u25A1");
            strcat(CheatsEnableStr, ss);
        }
    };
};
class BookmarkOverlay : public tsl::Gui {
   public:
    BookmarkOverlay() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            // if (GameRunning == false)
            renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 15 * (2 + m_displayed_bookmark_lines + m_displayed_cheat_lines) + 5, a(0x7111));
            // else
            //     renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 110, a(0x7111));

            renderer->drawString(BookmarkLabels, false, 45, 15, 15, renderer->a(0xFFFF));

            renderer->drawString(Variables, false, 190, 15, 15, renderer->a(0xFFFF));

            renderer->drawString(MultiplierStr, false, 5, 15, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS == 60) TeslaFPS = 1;
        // Check if game process has been terminated
        dmntchtHasCheatProcess(&(m_debugger->m_dmnt));
        if (!m_debugger->m_dmnt) {
            cleanup_se_tools();
            tsl::goBack();
        };
        // snprintf(FPS_var_compressed_c, sizeof FPS_compressed_c, "%u\n%2.2f", FPS, FPSavg);
        // snprintf(BookmarkLabels,sizeof BookmarkLabels,"label\nlabe\nGame Runing = %d\n%s\nSaltySD = %d\ndmntchtCheck = 0x%08x\n",GameRunning,bookmarkfilename,SaltySD,dmntchtCheck);
        // snprintf(Variables,sizeof Variables, "100\n200\n\n\n\n\n");
        // strcat(BookmarkLabels,bookmarkfilename);
        snprintf(BookmarkLabels, sizeof BookmarkLabels, "\n");
        snprintf(Variables, sizeof Variables, "\n");
        snprintf(Cursor, sizeof Cursor, "\n");
        snprintf(MultiplierStr, sizeof MultiplierStr, "Hold Left Stick & Right Stick to Exit\n");
        m_displayed_bookmark_lines = 0;
        // BookmarkLabels[0]=0;
        // Variables[0]=0;
        // snprintf(Variables, sizeof Variables, "%d\n%d\n%d\n%s\n%s", Bstate.A, Bstate.B, TeslaFPS, skin_temperature_c, Rotation_SpeedLevel_c);
        for (u8 line = 0; line < NUM_bookmark; line++) {
            if ((line + m_addresslist_offset) >= (m_AttributeDumpBookmark->size() / sizeof(bookmark_t)))
                break;

            // std::stringstream ss;
            // ss.str("");
            char ss[200] = "";
            bookmark_t bookmark;
            // if (line < NUM_bookmark)  // && (m_memoryDump->size() / sizeof(u64)) != 8)
            {
                u64 address = 0;
                // m_memoryDump->getData((line + m_addresslist_offset) * sizeof(u64), &address, sizeof(u64));
                // return;
                m_AttributeDumpBookmark->getData((line + m_addresslist_offset) * sizeof(bookmark_t), &bookmark, sizeof(bookmark_t));
				if (bookmark.magic!=0x1289) bookmark.multiplier = 1;
                // if (false)
                if (bookmark.pointer.depth > 0)  // check if pointer chain point to valid address update address if necessary
                {
                    bool updateaddress = true;
                    u64 nextaddress = metadata.main_nso_extents.base;  //m_mainBaseAddr;
                    for (int z = bookmark.pointer.depth; z >= 0; z--) {
                        nextaddress += bookmark.pointer.offset[z];
                        MemoryInfo meminfo = m_debugger->queryMemory(nextaddress);
                        if (meminfo.perm == Perm_Rw)
                            if (z == 0) {
                                if (address == nextaddress)
                                    updateaddress = false;
                                else {
                                    address = nextaddress;
                                }
                            } else
                                m_debugger->readMemory(&nextaddress, ((m_32bitmode) ? sizeof(u32) : sizeof(u64)), nextaddress);
                        else {
                            updateaddress = false;
                            break;
                        }
                    }
                    // if (updateaddress) {
                    // 	m_memoryDump->putData((line + m_addresslist_offset) * sizeof(u64), &address, sizeof(u64));
                    // 	m_memoryDump->flushBuffer();
                    // }
                } else {
                    address = ((bookmark.heap) ? ((m_debugger->queryMemory(metadata.heap_extents.base).type == 0) ? metadata.alias_extents.base
                                                                                                                  : metadata.heap_extents.base)
                                               : metadata.main_nso_extents.base) +
                              bookmark.offset;
                };
                // bookmark display
                // WIP
                searchValue_t value = {0};

                m_debugger->readMemory(&value, dataTypeSizes[bookmark.type], address);
                if ((m_oldvalue[line]._u64 == 0) || (m_oldvalue[line]._s64 > value._s64))
                    m_oldvalue[line]._s64 = value._s64;
                else if (bookmark.multiplier != 1) {
                    switch (bookmark.type) {
                        case SEARCH_TYPE_FLOAT_32BIT:
                            if (m_oldvalue[line]._f32 < value._f32) {
                                m_oldvalue[line]._f32 = (value._f32 - m_oldvalue[line]._f32) * bookmark.multiplier + m_oldvalue[line]._f32;
                            };
                            break;
                        case SEARCH_TYPE_FLOAT_64BIT:
                            if (m_oldvalue[line]._f64 < value._f64) {
                                m_oldvalue[line]._f64 = (value._f64 - m_oldvalue[line]._f64) * bookmark.multiplier + m_oldvalue[line]._f64;
                            };
                            break;
                        case SEARCH_TYPE_UNSIGNED_8BIT:
                        case SEARCH_TYPE_UNSIGNED_16BIT:
                        case SEARCH_TYPE_UNSIGNED_32BIT:
                        case SEARCH_TYPE_UNSIGNED_64BIT:
                            if (m_oldvalue[line]._u64 < value._u64) {
                                m_oldvalue[line]._u64 = (value._u64 - m_oldvalue[line]._u64) * bookmark.multiplier + m_oldvalue[line]._u64;
                            };
                            break;
                        default:
                            if (m_oldvalue[line]._s64 < value._s64) {
                                m_oldvalue[line]._s64 = (value._s64 - m_oldvalue[line]._s64) * bookmark.multiplier + m_oldvalue[line]._s64;
                            };
                            break;
                    };
                    m_debugger->writeMemory(&(m_oldvalue[line]), dataTypeSizes[bookmark.type], address);
                };
                snprintf(ss, sizeof ss, "%s\n", _getAddressDisplayString(address, m_debugger, (searchType_t)bookmark.type).c_str());
                strcat(Variables, ss);
                snprintf(ss, sizeof ss, "%s\n", bookmark.label);
                strcat(BookmarkLabels, ss);
                snprintf(ss, sizeof ss, "\n");
                strcat(Cursor, ss);
                snprintf(ss, sizeof ss, (bookmark.multiplier != 1) ? "X%02d\n" : "\n", bookmark.multiplier);
                strcat(MultiplierStr, ss);
                m_displayed_bookmark_lines++;
                // 	ss << "[0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(10) << (address) << "]";  //<< std::left << std::setfill(' ') << std::setw(18) << bookmark.label <<

                // 	ss << "  ( " << _getAddressDisplayString(address, m_debugger, (searchType_t)bookmark.type) << " )";

                // 	if (m_frozenAddresses.find(address) != m_frozenAddresses.end())
                // 		ss << " \uE130";
                // 	if (bookmark.pointer.depth > 0)  // have pointer
                // 		ss << " *";
            }
            // else {
            // 	snprintf(ss, sizeof ss, "%s\n", bookmark.label);
            // 	strcat(BookmarkLabels,ss);
            // 	ss << "And " << std::dec << ((m_memoryDump->size() / sizeof(u64)) - 8) << " others...";
            // }
            // Gui::drawRectangle(Gui::g_framebuffer_width - 555, 300 + line * 40, 545, 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? currTheme.highlightColor : line % 2 == 0 ? currTheme.backgroundColor
            // 																																													: currTheme.separatorColor);
            // Gui::drawTextAligned(font14, Gui::g_framebuffer_width - 545, 305 + line * 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? COLOR_BLACK : currTheme.textColor, bookmark.deleted ? "To be deleted" : bookmark.label, ALIGNED_LEFT);
            // Gui::drawTextAligned(font14, Gui::g_framebuffer_width - 340, 305 + line * 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? COLOR_BLACK : currTheme.textColor, ss.str().c_str(), ALIGNED_LEFT);
        }
        m_show_only_enabled_cheats = true;
        m_cheatlist_offset = 0;
        getcheats();
        strncat(BookmarkLabels,CheatsLabelsStr, sizeof BookmarkLabels-1);
        strncat(MultiplierStr,CheatsEnableStr, sizeof MultiplierStr-1);
    };
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            // CloseThreads();
            cleanup_se_tools();
            tsl::goBack();
            return true;
        };
        if (keysDown & HidNpadButton_B && keysHeld & HidNpadButton_ZL) {
            // CloseThreads();
            cleanup_se_tools();
            tsl::goBack();
            return true;
        }
        return false;
    }
};
//Mini mode
class MiniOverlay : public tsl::Gui {
   public:
    MiniOverlay() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            if (GameRunning == false)
                renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 80, a(0x7111));
            else
                renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 110, a(0x7111));

            //Print strings
            ///CPU
            if (GameRunning == true)
                renderer->drawString("CPU\nGPU\nRAM\nTEMP\nFAN\nPFPS\nFPS", false, 0, 15, 15, renderer->a(0xFFFF));
            else
                renderer->drawString("CPU\nGPU\nRAM\nTEMP\nFAN", false, 0, 15, 15, renderer->a(0xFFFF));

            ///GPU
            renderer->drawString(Variables, false, 60, 15, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS == 60) TeslaFPS = 1;
        //In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
        //This is because making each loop also takes time, which is not considered because this will take also additional time
        if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
        if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
        if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
        if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;

        //Make stuff ready to print
        ///CPU
        double percent = ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage0, sizeof CPU_Usage0, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage1, sizeof CPU_Usage1, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage2, sizeof CPU_Usage2, "%.0f%s", percent, "%");
        percent = ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100;
        snprintf(CPU_Usage3, sizeof CPU_Usage3, "%.0f%s", percent, "%");
        snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "[%s,%s,%s,%s]@%.1f", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3, (float)CPU_Hz / 1000000);

        ///GPU
        snprintf(GPU_Load_c, sizeof GPU_Load_c, "%.1f%s@%.1f", (float)GPU_Load_u / 10, "%", (float)GPU_Hz / 1000000);

        ///RAM
        float RAM_Total_application_f = (float)RAM_Total_application_u / 1024 / 1024;
        float RAM_Total_applet_f = (float)RAM_Total_applet_u / 1024 / 1024;
        float RAM_Total_system_f = (float)RAM_Total_system_u / 1024 / 1024;
        float RAM_Total_systemunsafe_f = (float)RAM_Total_systemunsafe_u / 1024 / 1024;
        float RAM_Total_all_f = RAM_Total_application_f + RAM_Total_applet_f + RAM_Total_system_f + RAM_Total_systemunsafe_f;
        float RAM_Used_application_f = (float)RAM_Used_application_u / 1024 / 1024;
        float RAM_Used_applet_f = (float)RAM_Used_applet_u / 1024 / 1024;
        float RAM_Used_system_f = (float)RAM_Used_system_u / 1024 / 1024;
        float RAM_Used_systemunsafe_f = (float)RAM_Used_systemunsafe_u / 1024 / 1024;
        float RAM_Used_all_f = RAM_Used_application_f + RAM_Used_applet_f + RAM_Used_system_f + RAM_Used_systemunsafe_f;
        snprintf(RAM_all_c, sizeof RAM_all_c, "%.0f/%.0fMB", RAM_Used_all_f, RAM_Total_all_f);
        snprintf(RAM_var_compressed_c, sizeof RAM_var_compressed_c, "%s@%.1f", RAM_all_c, (float)RAM_Hz / 1000000);

        ///Thermal
        snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2.1f\u00B0C/%2.1f\u00B0C/%2.1f\u00B0C", (float)SoC_temperaturemiliC / 1000, (float)PCB_temperaturemiliC / 1000, (float)skin_temperaturemiliC / 1000);
        snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "%2.2f%s", Rotation_SpeedLevel_f * 100, "%");

        ///FPS
        snprintf(FPS_c, sizeof FPS_c, "PFPS:");       //Pushed Frames Per Second
        snprintf(FPSavg_c, sizeof FPSavg_c, "FPS:");  //Frames Per Second calculated from averaged frametime
        snprintf(FPS_compressed_c, sizeof FPS_compressed_c, "%s\n%s", FPS_c, FPSavg_c);
        snprintf(FPS_var_compressed_c, sizeof FPS_compressed_c, "%u\n%2.2f", FPS, FPSavg);

        if (GameRunning == true)
            snprintf(Variables, sizeof Variables, "%s\n%s\n%s\n%s\n%s\n%s", CPU_compressed_c, GPU_Load_c, RAM_var_compressed_c, skin_temperature_c, Rotation_SpeedLevel_c, FPS_var_compressed_c);
        else
            snprintf(Variables, sizeof Variables, "%s\n%s\n%s\n%s\n%s", CPU_compressed_c, GPU_Load_c, RAM_var_compressed_c, skin_temperature_c, Rotation_SpeedLevel_c);
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            CloseThreads();
            tsl::goBack();
            return true;
        }
        return false;
    }
};

#ifdef CUSTOM
void BatteryChecker(void *) {
    if (R_SUCCEEDED(psmCheck)) {
        _batteryChargeInfoFields = new BatteryChargeInfoFields;
        while (!threadexit) {
            psmGetBatteryChargeInfoFields(psmService, _batteryChargeInfoFields);
            svcSleepThread(5'000'000'000);
        }
        delete _batteryChargeInfoFields;
    }
}

void StartBatteryThread() {
    threadCreate(&t7, BatteryChecker, NULL, NULL, 0x4000, 0x3F, 3);
    threadStart(&t7);
}

//CustomOverlay
class CustomOverlay : public tsl::Gui {
   public:
    CustomOverlay() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("Custom Overlay", APP_VERSION);

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            //Print strings
            ///CPU
            if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) {
                renderer->drawString("CPU Usage:", false, 20, 120, 20, renderer->a(0xFFFF));
                renderer->drawString(CPU_Hz_c, false, 20, 155, 15, renderer->a(0xFFFF));
                renderer->drawString(CPU_compressed_c, false, 20, 185, 15, renderer->a(0xFFFF));
            }

            ///GPU
            if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(nvCheck)) {
                renderer->drawString("GPU Usage:", false, 20, 285, 20, renderer->a(0xFFFF));
                if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(GPU_Hz_c, false, 20, 320, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(nvCheck)) renderer->drawString(GPU_Load_c, false, 20, 335, 15, renderer->a(0xFFFF));
            }

            ///RAM
            if (R_SUCCEEDED(psmCheck)) {
                renderer->drawString("Battery Stats:", false, 20, 375, 20, renderer->a(0xFFFF));
                renderer->drawString(Battery_c, false, 20, 410, 15, renderer->a(0xFFFF));
            }

            ///Thermal
            if (R_SUCCEEDED(tsCheck) || R_SUCCEEDED(tcCheck) || R_SUCCEEDED(fanCheck)) {
                renderer->drawString("Thermal:", false, 20, 540, 20, renderer->a(0xFFFF));
                if (R_SUCCEEDED(tsCheck)) renderer->drawString(SoCPCB_temperature_c, false, 20, 575, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(tcCheck)) renderer->drawString(skin_temperature_c, false, 20, 605, 15, renderer->a(0xFFFF));
                if (R_SUCCEEDED(fanCheck)) renderer->drawString(Rotation_SpeedLevel_c, false, 20, 620, 15, renderer->a(0xFFFF));
            }

            if (refreshrate == 5)
                renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Down to slow down refresh", false, 20, 675, 15, renderer->a(0xFFFF));
            else if (refreshrate == 1)
                renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Up to speed up refresh", false, 20, 675, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS == 60) TeslaFPS = 1;
        //In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
        //This is because making each loop also takes time, which is not considered because this will take also additional time
        if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
        if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
        if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
        if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;

        //Make stuff ready to print
        ///CPU
        snprintf(CPU_Hz_c, sizeof CPU_Hz_c, "Frequency: %.1f MHz", (float)CPU_Hz / 1000000);
        snprintf(CPU_Usage0, sizeof CPU_Usage0, "Core #0: %.2f%s", ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage1, sizeof CPU_Usage1, "Core #1: %.2f%s", ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage2, sizeof CPU_Usage2, "Core #2: %.2f%s", ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_Usage3, sizeof CPU_Usage3, "Core #3: %.2f%s", ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100, "%");
        snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "%s\n%s\n%s\n%s", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3);

        ///GPU
        snprintf(GPU_Hz_c, sizeof GPU_Hz_c, "Frequency: %.1f MHz", (float)GPU_Hz / 1000000);
        snprintf(GPU_Load_c, sizeof GPU_Load_c, "Load: %.1f%s", (float)GPU_Load_u / 10, "%");

        ///Battery
        snprintf(Battery_c, sizeof Battery_c,
                 "Battery Temperature: %.1f\u00B0C\n"
                 "Raw Battery Charge: %.1f%s\n"
                 "Voltage Avg: %u mV\n"
                 "Charger Type: %u\n",
                 (float)_batteryChargeInfoFields->BatteryTemperature / 1000,
                 (float)_batteryChargeInfoFields->RawBatteryCharge / 1000, "%",
                 _batteryChargeInfoFields->VoltageAvg,
                 _batteryChargeInfoFields->ChargerType);

        ///Thermal
        snprintf(SoCPCB_temperature_c, sizeof SoCPCB_temperature_c, "SoC: %2.2f \u00B0C\nPCB: %2.2f \u00B0C", (float)SoC_temperaturemiliC / 1000, (float)PCB_temperaturemiliC / 1000);
        snprintf(skin_temperature_c, sizeof skin_temperature_c, "Skin: %2.2f \u00B0C", (float)skin_temperaturemiliC / 1000);
        snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "Fan: %2.2f%s", Rotation_SpeedLevel_f * 100, "%");
    }
    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_RSTICK)) {
            CloseThreads();
            tsl::goBack();
            return true;
        }
        return false;
    }
};
#endif
class SetMultiplierOverlay : public tsl::Gui {
   public:
    SetMultiplierOverlay() {
        m_cheatlist_offset = 0;
    }
    
    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");

        auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
            // if (GameRunning == false)
            renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth , tsl::cfg::FramebufferHeight, a(0x7111));
            // else
            //     renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 110, a(0x7111));

            renderer->drawString(BookmarkLabels, false, 65, 15, 15, renderer->a(0xFFFF));

            renderer->drawString(Variables, false, 210, 15, 15, renderer->a(0xFFFF));

            renderer->drawString(Cursor, false, 5, 15, 15, renderer->a(0xFFFF));

            renderer->drawString(MultiplierStr, false, 25, 15, 15, renderer->a(0xFFFF));
        });

        rootFrame->setContent(Status);

        return rootFrame;
    }

    virtual void update() override {
        // if (TeslaFPS == 60) TeslaFPS = 1;

        // snprintf(FPS_var_compressed_c, sizeof FPS_compressed_c, "%u\n%2.2f", FPS, FPSavg);
        // snprintf(BookmarkLabels,sizeof BookmarkLabels,"label\nlabe\nGame Runing = %d\n%s\nSaltySD = %d\ndmntchtCheck = 0x%08x\n",GameRunning,bookmarkfilename,SaltySD,dmntchtCheck);
        // snprintf(Variables,sizeof Variables, "100\n200\n\n\n\n\n");
        // strcat(BookmarkLabels,bookmarkfilename);
        snprintf(BookmarkLabels, sizeof BookmarkLabels, "\n");
        snprintf(Variables, sizeof Variables, "\n");
        snprintf(Cursor, sizeof Cursor, "\uE092\uE093    \uE0A4 \uE0A5 change     \uE0A0 toggle     \uE0A1 exit\n");
        snprintf(MultiplierStr, sizeof MultiplierStr, "\n");
        // BookmarkLabels[0]=0;
        // Variables[0]=0;
        // snprintf(Variables, sizeof Variables, "%d\n%d\n%d\n%s\n%s", Bstate.A, Bstate.B, TeslaFPS, skin_temperature_c, Rotation_SpeedLevel_c);
        for (u8 line = 0; line < NUM_bookmark; line++) {
            if ((line + m_addresslist_offset) >= (m_AttributeDumpBookmark->size() / sizeof(bookmark_t)))
                break;

            // std::stringstream ss;
            // ss.str("");
            char ss[200] = "";
            bookmark_t bookmark;

            // if (line < NUM_bookmark)  // && (m_memoryDump->size() / sizeof(u64)) != 8)
            {
                u64 address = 0;
                // m_memoryDump->getData((line + m_addresslist_offset) * sizeof(u64), &address, sizeof(u64));
                // return;
                m_AttributeDumpBookmark->getData((line + m_addresslist_offset) * sizeof(bookmark_t), &bookmark, sizeof(bookmark_t));
				if (bookmark.magic!=0x1289) bookmark.multiplier = 1;
                // if (false)
                if (bookmark.pointer.depth > 0)  // check if pointer chain point to valid address update address if necessary
                {
                    bool updateaddress = true;
                    u64 nextaddress = metadata.main_nso_extents.base;  //m_mainBaseAddr;
                    for (int z = bookmark.pointer.depth; z >= 0; z--) {
                        nextaddress += bookmark.pointer.offset[z];
                        MemoryInfo meminfo = m_debugger->queryMemory(nextaddress);
                        if (meminfo.perm == Perm_Rw)
                            if (z == 0) {
                                if (address == nextaddress)
                                    updateaddress = false;
                                else {
                                    address = nextaddress;
                                }
                            } else
                                m_debugger->readMemory(&nextaddress, ((m_32bitmode) ? sizeof(u32) : sizeof(u64)), nextaddress);
                        else {
                            updateaddress = false;
                            break;
                        }
                    }
                    // if (updateaddress) {
                    //     m_memoryDump->putData((line + m_addresslist_offset) * sizeof(u64), &address, sizeof(u64));
                    //     m_memoryDump->flushBuffer();
                    // }
                } else {
                    address = ((bookmark.heap) ? ((m_debugger->queryMemory(metadata.heap_extents.base).type == 0) ? metadata.alias_extents.base
                                                                                                                  : metadata.heap_extents.base)
                                               : metadata.main_nso_extents.base) +
                              bookmark.offset;
                };
                // bookmark display
                snprintf(ss, sizeof ss, "%s\n", _getAddressDisplayString(address, m_debugger, (searchType_t)bookmark.type).c_str());
                strcat(Variables, ss);
                snprintf(ss, sizeof ss, "%s\n", bookmark.label);
                strcat(BookmarkLabels, ss);
                snprintf(ss, sizeof ss, "%s\n", ((m_index == line) && m_cursor_on_bookmark) ? "\uE019" : "");
                strcat(Cursor, ss);
                snprintf(ss, sizeof ss, "X%02d\n", bookmark.multiplier);
                strcat(MultiplierStr, ss);
                // 	ss << "[0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(10) << (address) << "]";  //<< std::left << std::setfill(' ') << std::setw(18) << bookmark.label <<

                // 	ss << "  ( " << _getAddressDisplayString(address, m_debugger, (searchType_t)bookmark.type) << " )";

                // 	if (m_frozenAddresses.find(address) != m_frozenAddresses.end())
                // 		ss << " \uE130";
                // 	if (bookmark.pointer.depth > 0)  // have pointer
                // 		ss << " *";
            }
            // else {
            // 	snprintf(ss, sizeof ss, "%s\n", bookmark.label);
            // 	strcat(BookmarkLabels,ss);
            // 	ss << "And " << std::dec << ((m_memoryDump->size() / sizeof(u64)) - 8) << " others...";
            // }
            // Gui::drawRectangle(Gui::g_framebuffer_width - 555, 300 + line * 40, 545, 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? currTheme.highlightColor : line % 2 == 0 ? currTheme.backgroundColor
            // 																																													: currTheme.separatorColor);
            // Gui::drawTextAligned(font14, Gui::g_framebuffer_width - 545, 305 + line * 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? COLOR_BLACK : currTheme.textColor, bookmark.deleted ? "To be deleted" : bookmark.label, ALIGNED_LEFT);
            // Gui::drawTextAligned(font14, Gui::g_framebuffer_width - 340, 305 + line * 40, (m_selectedEntry == line && m_menuLocation == CANDIDATES) ? COLOR_BLACK : currTheme.textColor, ss.str().c_str(), ALIGNED_LEFT);
        }
        // print cheats
        if (m_AttributeDumpBookmark->size() == 0) m_cursor_on_bookmark = false;
        m_show_only_enabled_cheats = false;
        // m_cheatlist_offset = 0;
        getcheats();
        strncat(BookmarkLabels,CheatsLabelsStr, sizeof BookmarkLabels-1);
        strncat(Cursor,CheatsCursor, sizeof Cursor-1);
        strncat(MultiplierStr,CheatsEnableStr, sizeof MultiplierStr-1);
    };
    // WIP Setting
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if ((keysHeld & HidNpadButton_StickL) && (keysHeld & HidNpadButton_StickR)) {
            // CloseThreads();
            cleanup_se_tools();
            tsl::goBack();
            return true;
        };
        if ((keysDown & HidNpadButton_AnyUp) || (keysHeld & HidNpadButton_StickRUp)) {
            if (m_cursor_on_bookmark) {
                if (m_index > 0) m_index--;
            } else {
                if (m_cheat_index > 0)
                    m_cheat_index--;
                else {
                    if (m_cheatlist_offset > 0)
                        m_cheatlist_offset--;
                    else
                        if (m_AttributeDumpBookmark->size() > 0) m_cursor_on_bookmark = true;
                }
            }
            return true;
        };
        if ((keysDown & HidNpadButton_AnyDown) || (keysHeld & HidNpadButton_StickRDown)) {
            if (m_cursor_on_bookmark) {
                if ((m_index < NUM_bookmark - 1) && ((m_index + m_addresslist_offset) < (m_AttributeDumpBookmark->size() / sizeof(bookmark_t) - 1))) m_index++;
                else if (m_cheatCnt > 0)
                    m_cursor_on_bookmark = false;
            } else {
                if ((m_cheat_index < NUM_cheats - 1) && ((m_cheat_index + m_cheatlist_offset) < m_cheatCnt - 1)) m_cheat_index++;
                else if ((m_cheat_index + m_cheatlist_offset) < m_cheatCnt - 1)
                    m_cheatlist_offset++;
            }
            return true;
        };
        if (((keysDown & HidNpadButton_L) || (keysDown & HidNpadButton_R)) && m_cursor_on_bookmark) {
            bookmark_t bookmark;
            m_AttributeDumpBookmark->getData((m_index + m_addresslist_offset) * sizeof(bookmark_t), &bookmark, sizeof(bookmark_t));
			if (bookmark.magic!=0x1289) bookmark.multiplier = 1;
            if (keysDown & HidNpadButton_R) {
                switch (bookmark.multiplier) {
                    case 1:
                        bookmark.multiplier = 2;
                        break;
                    case 2:
                        bookmark.multiplier = 4;
                        break;
                    case 4:
                        bookmark.multiplier = 8;
                        break;
                    case 8:
                        bookmark.multiplier = 16;
                        break;
                    case 16:
                        bookmark.multiplier = 32;
                        break;
                    case 32:
                        break;
                    default:
                        bookmark.multiplier = 1;
                        break;
                };
            } else {
                switch (bookmark.multiplier) {
                    case 32:
                        bookmark.multiplier = 16;
                        break;
                    case 16:
                        bookmark.multiplier = 8;
                        break;
                    case 8:
                        bookmark.multiplier = 4;
                        break;
                    case 4:
                        bookmark.multiplier = 2;
                        break;
                    default:
                        bookmark.multiplier = 1;
                        break;
                };
            };
			bookmark.magic = 0x1289;
            m_AttributeDumpBookmark->putData((m_index + m_addresslist_offset) * sizeof(bookmark_t), &bookmark, sizeof(bookmark_t));
            return true;
        };
        if ((keysDown & HidNpadButton_A) && !m_cursor_on_bookmark) {
            if (m_cheats[m_cheat_index + m_cheatlist_offset].enabled)
                dmntchtToggleCheat(m_cheats[m_cheat_index + m_cheatlist_offset].cheat_id);
            else {
                if (m_cheats[m_cheat_index + m_cheatlist_offset].definition.num_opcodes + total_opcode <= MaximumProgramOpcodeCount)
                    dmntchtToggleCheat(m_cheats[m_cheat_index + m_cheatlist_offset].cheat_id);
            }
            refresh_cheats = true;
            return true;
        }
        if (keysDown & HidNpadButton_AnyLeft) {
            if (m_AttributeDumpBookmark->size() > 0) m_cursor_on_bookmark = true; 
            return true;
        }
        if (keysDown & HidNpadButton_AnyRight) {
            if (m_cheatCnt > 0)
                    m_cursor_on_bookmark = false;
            return true;
        }
        if (keysDown & HidNpadButton_ZR) {
            if (!m_cursor_on_bookmark) {
                if ((m_cheatlist_offset + NUM_cheats) < m_cheatCnt - 1) m_cheatlist_offset += NUM_cheats;
                if ((m_cheat_index + m_cheatlist_offset) > m_cheatCnt - 1) m_cheat_index = m_cheatCnt - 1 - m_cheatlist_offset;
            };
            return true;
        }
        if (keysDown & HidNpadButton_ZL) {
            if (!m_cursor_on_bookmark) {
                if (m_cheatlist_offset > NUM_cheats) m_cheatlist_offset -= NUM_cheats; else m_cheatlist_offset = 0;
            };
            return true;
        }
        if (keysDown & HidNpadButton_B) {
            // CloseThreads();
            cleanup_se_tools();
            tsl::goBack();
            return true;
        }
        return false;
    }
};
class SetMultiplierOverlay2 : public tsl::Gui {
   public:
    SetMultiplierOverlay2() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("Breeze", APP_VERSION);
        auto list = new tsl::elm::List();

        auto Bookmark = new tsl::elm::ListItem("Show SE Bookmark");
        Bookmark->setClickListener([](uint64_t keys) {
            if (keys & HidNpadButton_A) {
                // StartThreads();
                init_se_tools();
                TeslaFPS = 1;
                refreshrate = 1;
                alphabackground = 0x0;
                tsl::hlp::requestForeground(false);
                FullMode = false;
                tsl::changeTo<BookmarkOverlay>();
                return true;
            }
            return false;
        });
        list->addItem(Bookmark);

        rootFrame->setContent(list);
        return rootFrame;
    }
    virtual void update() override {
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if (keysHeld & HidNpadButton_B) {
            tsl::goBack();
            return true;
        }
        if (keysHeld & HidNpadButton_X) {
            tsl::Overlay::get()->hide();
            return true;
        }
        return false;
    }
};
//Main Menu
class MainMenu : public tsl::Gui { // WIP
   public:
    MainMenu() {}

    virtual tsl::elm::Element *createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("Breeze", APP_VERSION);
        auto list = new tsl::elm::List();

        auto Bookmark = new tsl::elm::ListItem("Activate Monitor");
        Bookmark->setClickListener([](uint64_t keys) {
            if (keys & HidNpadButton_A) {
                // StartThreads();
                if (!init_se_tools()) return true;
                TeslaFPS = 20;
                refreshrate = 1;
                alphabackground = 0x0;
                tsl::hlp::requestForeground(false);
                FullMode = false;
                tsl::changeTo<BookmarkOverlay>();
                return true;
            }
            return false;
        });
        list->addItem(Bookmark);

        auto SetMultiplier = new tsl::elm::ListItem("Settings");
        SetMultiplier->setClickListener([](uint64_t keys) {
            if (keys & HidNpadButton_A) {
                // StartThreads();
                if (!init_se_tools()) return true;
                TeslaFPS = 50;
                refreshrate = 1;
                alphabackground = 0x0;
                tsl::hlp::requestForeground(true);
                FullMode = false;
                tsl::changeTo<SetMultiplierOverlay>();
                return true;
            }
            return false;
        });
        list->addItem(SetMultiplier);

        // auto MailBox = new tsl::elm::ListItem("MailBox");
        // MailBox->setClickListener([](uint64_t keys) {
        // 	if (keys & HidNpadButton_A) {
        // 		StartThreads();
        // 		TeslaFPS = 1;
        // 		refreshrate = 1;
        // 		alphabackground = 0x0;
        // 		tsl::hlp::requestForeground(false);
        // 		FullMode = false;
        // 		tsl::changeTo<MailBoxOverlay>();
        // 		return true;
        // 	}
        // 	return false;
        // });
        // list->addItem(MailBox);

        // auto Full = new tsl::elm::ListItem("Full");
        // Full->setClickListener([](uint64_t keys) {
        // 	if (keys & HidNpadButton_A) {
        // 		StartThreads();
        // 		TeslaFPS = 1;
        // 		refreshrate = 1;
        // 		tsl::hlp::requestForeground(false);
        // 		tsl::changeTo<FullOverlay>();
        // 		return true;
        // 	}
        // 	return false;
        // });
        // list->addItem(Full);

        // auto Mini = new tsl::elm::ListItem("Mini");
        // Mini->setClickListener([](uint64_t keys) {
        // 	if (keys & HidNpadButton_A) {
        // 		StartThreads();
        // 		TeslaFPS = 1;
        // 		refreshrate = 1;
        // 		alphabackground = 0x0;
        // 		tsl::hlp::requestForeground(false);
        // 		FullMode = false;
        // 		tsl::changeTo<MiniOverlay>();
        // 		return true;
        // 	}
        // 	return false;
        // });
        // list->addItem(Mini);

        if (SaltySD == true) {
            auto comFPS = new tsl::elm::ListItem("FPS Counter");
            comFPS->setClickListener([](uint64_t keys) {
                if (keys & HidNpadButton_A) {
                    StartFPSCounterThread();
                    TeslaFPS = 31;
                    refreshrate = 31;
                    alphabackground = 0x0;
                    tsl::hlp::requestForeground(false);
                    FullMode = false;
                    tsl::changeTo<com_FPS>();
                    return true;
                }
                return false;
            });
            list->addItem(comFPS);
        }
#ifdef CUSTOM
        auto Custom = new tsl::elm::ListItem("Custom");
        Custom->setClickListener([](uint64_t keys) {
            if (keys & HidNpadButton_A) {
                StartThreads();
                StartBatteryThread();
                TeslaFPS = 1;
                refreshrate = 1;
                tsl::hlp::requestForeground(false);
                tsl::changeTo<CustomOverlay>();
                return true;
            }
            return false;
        });
        list->addItem(Custom);
#endif

        rootFrame->setContent(list);

        return rootFrame;
    }

    virtual void update() override {
        if (TeslaFPS != 60) {
            FullMode = true;
            tsl::hlp::requestForeground(true);
            TeslaFPS = 60;
            alphabackground = 0xD;
            refreshrate = 1;
            systemtickfrequency = 19200000;
        }
        if (Bstate.A == 123) {
            tsl::hlp::requestForeground(true);
            Bstate.A += 100;
        };
        Bstate.B += 1;
    }
    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        if (keysDown & HidNpadButton_B) {
            tsl::goBack();
            return true;
        }
        if (keysHeld & HidNpadButton_X) {
            tsl::Overlay::get()->hide();
            return true;
        }
        return false;
    }
};

class MonitorOverlay : public tsl::Overlay {
   public:
    virtual void initServices() override {
        dmntchtCheck = dmntchtInitialize();
        //Initialize services
        if (R_SUCCEEDED(smInitialize())) {
            if (hosversionAtLeast(8, 0, 0))
                clkrstCheck = clkrstInitialize();
            else
                pcvCheck = pcvInitialize();

            tsCheck = tsInitialize();
            if (hosversionAtLeast(5, 0, 0)) tcCheck = tcInitialize();

            if (R_SUCCEEDED(fanInitialize())) {
                if (hosversionAtLeast(7, 0, 0))
                    fanCheck = fanOpenController(&g_ICon, 0x3D000001);
                else
                    fanCheck = fanOpenController(&g_ICon, 1);
            }

            if (R_SUCCEEDED(nvInitialize())) nvCheck = nvOpen(&fd, "/dev/nvhost-ctrl-gpu");

#ifdef CUSTOM
            psmCheck = psmInitialize();
            if (R_SUCCEEDED(psmCheck)) psmService = psmGetServiceSession();
#endif

            // Atmosphere_present = isServiceRunning("dmnt:cht");
            // SaltySD = CheckPort();
            // if (SaltySD == true && Atmosphere_present == true) dmntchtCheck = dmntchtInitialize();

            if (SaltySD == true) {
                //Assign NX-FPS to default core
                threadCreate(&t6, CheckIfGameRunning, NULL, NULL, 0x1000, 0x38, -2);

                //Start NX-FPS detection
                threadStart(&t6);
            }
            smExit();
        }
        Hinted = envIsSyscallHinted(0x6F);
    }

    virtual void exitServices() override {
        CloseThreads();

        //Exit services
        svcCloseHandle(debug);
        dmntchtExit();
        clkrstExit();
        pcvExit();
        tsExit();
        tcExit();
        fanControllerClose(&g_ICon);
        fanExit();
        nvClose(fd);
        nvExit();
#ifdef CUSTOM
        psmExit();
#endif
    }

    virtual void onShow() override {}  // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {}  // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainMenu>();  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

// This function gets called on startup to create a new Overlay object
int main(int argc, char **argv) {
    return tsl::loop<MonitorOverlay>(argc, argv);
}
