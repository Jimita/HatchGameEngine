#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/InputManager.h>
#include <Engine/Audio/AudioManager.h>
#include <Engine/Scene.h>
#include <Engine/Math/Math.h>
#include <Engine/TextFormats/INI.h>

class Application {
public:
    static INI*        Settings;

    static float       FPS;
    static bool        Running;

    static SDL_Window* Window;
    static Platforms   Platform;
};
#endif

#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#include <Engine/Media/MediaSource.h>
#include <Engine/Media/MediaPlayer.h>

#if   WIN32
    Platforms Application::Platform = Platforms::Windows;
#elif MACOSX
    Platforms Application::Platform = Platforms::MacOSX;
#elif LINUX
    Platforms Application::Platform = Platforms::Linux;
#elif UBUNTU
    Platforms Application::Platform = Platforms::Ubuntu;
#elif SWITCH
    Platforms Application::Platform = Platforms::Switch;
#elif PLAYSTATION
    Platforms Application::Platform = Platforms::Playstation;
#elif XBOX
    Platforms Application::Platform = Platforms::Xbox;
#elif ANDROID
    Platforms Application::Platform = Platforms::Android;
#elif IOS
    Platforms Application::Platform = Platforms::iOS;
#else
    Platforms Application::Platform = Platforms::Default;
#endif

INI*        Application::Settings = NULL;

float       Application::FPS = 60.f;
int         TargetFPS = 60;
bool        Application::Running = false;

SDL_Window* Application::Window = NULL;

char        StartingScene[256];

ISprite*    DEBUG_fontSprite = NULL;
void        DEBUG_DrawText(char* text, float x, float y) {
    for (size_t i = 0; i < strlen(text); i++) {
        Graphics::DrawSprite(DEBUG_fontSprite, 0, (int)text[i], x, y, 0, 0);
        x += DEBUG_fontSprite->Animations[0].Frames[(int)text[i]].ID;
    }
}

PUBLIC STATIC void Application::Init(int argc, char* args[]) {
    Log::Init();

    // SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown"); // iOS only
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    #ifdef ANDROID
    SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
    #endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        Log::Print(Log::LOG_INFO, "SDL_Init failed with error: %s", SDL_GetError());
    }

    SDL_SetEventFilter(Application::HandleAppEvents, NULL);

    Application::LoadSettings();

    Graphics::ChooseBackend();

    bool allowRetina = false;
    Application::Settings->GetBool("display", "retina", &allowRetina);

    Uint32 window_flags = 0;
    window_flags |= SDL_WINDOW_SHOWN;
    window_flags |= Graphics::GetWindowFlags();
    if (allowRetina)
        window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    Application::Window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, window_flags);

    if (Application::Platform == Platforms::Switch) {
        SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
        // SDL_GetWindowSize(Application::Window, &Application::WIDTH, &Application::HEIGHT);
        AudioManager::MasterVolume = 0.25;

        #ifdef SWITCH
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, 1 - appletGetOperationMode(), &mode);
        // SDL_SetWindowDisplayMode(Application::Window, &mode);
        Log::Print(Log::LOG_INFO, "Display Mode: %i x %i", mode.w, mode.h);
        #endif
    }

    if (Application::Platform == Platforms::Android) {
        Graphics::VsyncEnabled = true;
    }

    // Initialize subsystems
    Math::Init();
    Graphics::Init();
    if (argc > 1 && !!strstr(args[1], ".hatch"))
        ResourceManager::Init(args[1]);
    else
        ResourceManager::Init(NULL);
    AudioManager::Init();
    InputManager::Init();
    Clock::Init();

    Application::LoadGameConfig();

    switch (Application::Platform) {
        case Platforms::Windows:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Windows"); break;
        case Platforms::MacOSX:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "MacOS"); break;
        case Platforms::Linux:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Linux"); break;
        case Platforms::Ubuntu:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Ubuntu"); break;
        case Platforms::Switch:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Nintendo Switch"); break;
        case Platforms::Playstation:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Playstation"); break;
        case Platforms::Xbox:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Xbox"); break;
        case Platforms::Android:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Android"); break;
        case Platforms::iOS:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "iOS"); break;
        default:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Unknown"); break;
    }

    char WindowTitle[256];
    sprintf(WindowTitle, "%s%s", "Hatch Game Engine", ResourceManager::UsingDataFolder ? " (using Resources folder)" : "");
    SDL_SetWindowTitle(Application::Window, WindowTitle);

    Running = true;
}

PUBLIC STATIC void Application::Run(int argc, char* args[]) {
    Application::Init(argc, args);
    if (!Running)
        return;

    bool    DevMenu = true;
    bool    ShowFPS = false;
    bool    TakeSnapshot = false;
    bool    DoNothing = false;
    int     UpdatesPerFrame = 1;
    int     UpdatesPerFastForward = 4;

    Application::Settings->GetBool("dev", "devMenu", &DevMenu);
    Application::Settings->GetBool("dev", "viewPerformance", &ShowFPS);
    Application::Settings->GetBool("dev", "donothing", &DoNothing);
    Application::Settings->GetInteger("dev", "fastforward", &UpdatesPerFastForward);

    Scene::Init();
    if (argc > 1 && !!strstr(args[1], ".tmx")) {
        char cwd[512];
        if (Directory::GetCurrentWorkingDirectory(cwd, sizeof(cwd))) {
            if (!!strstr(args[1], "/Resources/") || !!strstr(args[1], "\\Resources\\")) {
                char* tmxPath = args[1] + strlen(cwd) + strlen("/Resources/");
                for (char* i = tmxPath; *i; i++) {
                    if (*i == '\\')
                        *i = '/';
                }
                Scene::LoadScene(tmxPath);
            }
            else {
                Log::Print(Log::LOG_WARN, "Map file \"%s\" not inside Resources folder!", args[1]);
            }
        }
    }
    else if (*StartingScene) {
        Scene::LoadScene(StartingScene);
    }
    Scene::LoadTileCollisions("Scenes/TileCol.bin");

    Scene::Restart();

    bool    Stepper = false;
    bool    Step = false;

    int     MetricFrameCounterTime = 0;
    double  MetricEventTime = -1;
    double  MetricPollTime = -1;
    double  MetricUpdateTime = -1;
    double  MetricRenderTime = -1;
    double  MetricClearTime = -1;
    double  MetricPresentTime = -1;
    int     BenchmarkFrameCount = 0;
    double  Overdelay = 0.0;

    Graphics::Clear();
    Graphics::Present();

    SDL_Event e;
    while (Running) {
        if (BenchmarkFrameCount == 0)
            Clock::Start();

        if (!Graphics::VsyncEnabled || Application::Platform == Platforms::MacOSX)
            Clock::Start();

        // Event loop
        MetricEventTime = Clock::GetTicks();
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: {
                    Running = false;
                    break;
                }
                case SDL_KEYDOWN: {
                    switch (e.key.keysym.sym) {
                        // Quit game
                        case SDLK_ESCAPE: {
                            Running = false;
                            break;
                        }
                        // Fullscreen
                        case SDLK_F4: {
                            if (SDL_GetWindowFlags(Application::Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                                SDL_SetWindowFullscreen(Application::Window, 0);
                            }
                            else {
                                // SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
                                SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                            }
                            int ww, wh;
                            SDL_GetWindowSize(Application::Window, &ww, &wh);
                            Graphics::Resize(ww, wh);
                            break;
                        }
                        // Take snapshot
                        case SDLK_F10: {
                            TakeSnapshot = true;
                            break;
                        }
                        // Restart application (dev)
                        case SDLK_F1: {
                            if (DevMenu) {
                                // Reset FPS timer
                                BenchmarkFrameCount = 0;

                                Scene::Dispose();
                                Graphics::SpriteSheetTextureMap->Clear();

                                Application::LoadGameConfig();
                                Application::LoadSettings();

                                Application::Settings->GetBool("dev", "devMenu", &DevMenu);
                                Application::Settings->GetBool("dev", "viewPerformance", &ShowFPS);
                                Application::Settings->GetBool("dev", "donothing", &DoNothing);
                                Application::Settings->GetInteger("dev", "fastforward", &UpdatesPerFastForward);

                                Scene::Init();
                                if (*StartingScene)
                                    Scene::LoadScene(StartingScene);
                                Scene::LoadTileCollisions("Scenes/TileCol.bin");
                                Scene::Restart();
                            }
                            break;
                        }
                        // Restart scene (dev)
                        case SDLK_F5: {
                            if (DevMenu) {
                                // Reset FPS timer
                                BenchmarkFrameCount = 0;

                                Scene::Dispose();
                                Graphics::SpriteSheetTextureMap->Clear();

                                Application::LoadGameConfig();
                                Application::LoadSettings();

                                Application::Settings->GetBool("dev", "devMenu", &DevMenu);
                                Application::Settings->GetBool("dev", "viewPerformance", &ShowFPS);
                                Application::Settings->GetBool("dev", "donothing", &DoNothing);
                                Application::Settings->GetInteger("dev", "fastforward", &UpdatesPerFastForward);

                                char temp[256];
                                memcpy(temp, Scene::CurrentScene, 256);

                                Scene::Init();

                                memcpy(Scene::CurrentScene, temp, 256);
                                Scene::LoadScene(Scene::CurrentScene);

                                Scene::LoadTileCollisions("Scenes/TileCol.bin");
                                Scene::Restart();
                            }
                            break;
                        }
                        // Restart scene without recompiling (dev)
                        case SDLK_F6: {
                            if (DevMenu) {
                                // Reset FPS timer
                                BenchmarkFrameCount = 0;

                                Scene::Restart();
                            }
                            break;
                        }
                        // Enable update speedup (dev)
                        case SDLK_BACKSPACE: {
                            if (DevMenu) {
                                if (UpdatesPerFrame == 1)
                                    UpdatesPerFrame = 4;
                                else
                                    UpdatesPerFrame = 1;
                            }
                            break;
                        }
                        // Reset frame counter (dev)
                        case SDLK_F7: {
                            if (DevMenu) {
                                MetricFrameCounterTime = 0;
                            }
                            break;
                        }
                        // Toggle frame stepper (dev)
                        case SDLK_o:
                        case SDLK_F8: {
                            if (DevMenu) {
                                Stepper = !Stepper;
                                MetricFrameCounterTime = 0;
                            }
                            break;
                        }
                        // Step frame (dev)
                        case SDLK_p:
                        case SDLK_F9: {
                            if (DevMenu) {
                                Stepper = true;
                                Step = true;
                                MetricFrameCounterTime++;
                            }
                            break;
                        }
                    }
                    break;
                }
                case SDL_WINDOWEVENT: {
                    switch (e.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            Graphics::Resize(e.window.data1, e.window.data2);
                            break;
                    }
                    break;
                }
                case SDL_JOYDEVICEADDED: {
                    int i = e.jdevice.which;
                    InputManager::Controllers[i] = SDL_JoystickOpen(i);
                    if (InputManager::Controllers[i])
                        Log::Print(Log::LOG_VERBOSE, "InputManager::Controllers[%d] opened.", i);

                    if (InputManager::Controllers[i])
                        InputManager::ControllerHaptics[i] = SDL_HapticOpenFromJoystick((SDL_Joystick*)InputManager::Controllers[i]);

                    if (InputManager::ControllerHaptics[i] && SDL_HapticRumbleInit((SDL_Haptic*)InputManager::ControllerHaptics[i]))
                        InputManager::ControllerHaptics[i] = NULL;
                    break;
                }
                case SDL_JOYDEVICEREMOVED: {
                    int i = e.jdevice.which;

                    SDL_Joystick* joy = (SDL_Joystick*)InputManager::Controllers[i];

                    SDL_JoystickClose(joy);

                    InputManager::Controllers[i] = NULL;

                    // if (InputManager::Controllers[i])
                    //     InputManager::ControllerHaptics[i] = SDL_HapticOpenFromJoystick((SDL_Joystick*)InputManager::Controllers[i]);
                    //
                    // if (InputManager::ControllerHaptics[i] && SDL_HapticRumbleInit((SDL_Haptic*)InputManager::ControllerHaptics[i]))
                    //     InputManager::ControllerHaptics[i] = NULL;
                    break;
                }
            }
        }
        #ifdef SWITCH
            // Uint32 nxMsg;
            // while (R_SUCCEEDED(appletGetMessage(&nxMsg))) {
            //     switch (nxMsg) {
            //         case AppletMessage_OperationModeChanged:
            //             SDL_DisplayMode mode;
            //             SDL_GetDisplayMode(0, 1 - appletGetOperationMode(), &mode);
            //             SDL_SetWindowDisplayMode(Application::Window, &mode);
            //             Log::Print(Log::LOG_INFO, "Display Mode: %i x %i", mode.w, mode.h);
            //             break;
            //     }
            // }
        #endif
        MetricEventTime = Clock::GetTicks() - MetricEventTime;

        // NOTE: This fixes the bug where having Stepper on prevents the first
        //   frame of a new scene from Updating, but still rendering.
        if (*Scene::NextScene)
            Step = true;

        Scene::AfterScene();

        if (DoNothing) goto DO_NOTHING;

        // Update
        for (int m = 0; m < UpdatesPerFrame; m++) {
            if ((Stepper && Step) || !Stepper) {
                // Poll for inputs
                MetricPollTime = Clock::GetTicks();
                InputManager::Poll();
                MetricPollTime = Clock::GetTicks() - MetricPollTime;

                // Update scene
                MetricUpdateTime = Clock::GetTicks();
                Scene::Update();
                MetricUpdateTime = Clock::GetTicks() - MetricUpdateTime;
            }
            Step = false;
        }

        // Rendering
        MetricClearTime = Clock::GetTicks();
        Graphics::Clear();
        MetricClearTime = Clock::GetTicks() - MetricClearTime;

        MetricRenderTime = Clock::GetTicks();
        Scene::Render();
        MetricRenderTime = Clock::GetTicks() - MetricRenderTime;

        if (TakeSnapshot) {
            TakeSnapshot = false;
            // Graphics::SaveScreenshot(NULL);
        }

        DO_NOTHING:

        // Show FPS counter
        if (ShowFPS) {
            if (!DEBUG_fontSprite) {
                bool original = Graphics::TextureInterpolate;
                Graphics::SetTextureInterpolation(true);

                DEBUG_fontSprite = new ISprite();

                int cols, rows;
                DEBUG_fontSprite->SpritesheetCount = 1;
                DEBUG_fontSprite->Spritesheets[0] = DEBUG_fontSprite->AddSpriteSheet("Debug/Font.png");
                cols = DEBUG_fontSprite->Spritesheets[0]->Width / 32;
                rows = DEBUG_fontSprite->Spritesheets[0]->Height / 32;

                DEBUG_fontSprite->ReserveAnimationCount(1);
                DEBUG_fontSprite->AddAnimation("Font?", 0, 0, cols * rows);
                for (int i = 0; i < cols * rows; i++) {
                    DEBUG_fontSprite->AddFrame(0,
                        (i % cols) * 32,
                        (i / cols) * 32,
                        32, 32, 0, 0,
                        14);
                }

                Graphics::SetTextureInterpolation(original);
            }

            int ww, wh;
            char textBuffer[256];
            SDL_GetWindowSize(Application::Window, &ww, &wh);
            Graphics::SetViewport(0.0, 0.0, ww, wh);
            Graphics::UpdateOrthoFlipped(ww, wh);

            Graphics::SetBlendMode(BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA, BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA);

            float infoW = 400.0;
            float infoH = 235.0;
            float infoPadding = 20.0;
            Graphics::Save();
            Graphics::Translate(0.0, 0.0, 0.0);
                Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.75);
                Graphics::FillRectangle(0.0f, 0.0f, infoW, infoH);

                int typeCount = 5;

                double types[5] = {
                    MetricEventTime,
                    MetricPollTime,
                    MetricClearTime,
                    MetricUpdateTime,
                    MetricRenderTime,
                };
                const char* typeNames[5] = {
                    "Event Polling: %3.3f ms",
                    "Input Polling: %3.3f ms",
                    "Clear Time: %3.3f ms",
                    "Entity Update: %3.3f ms",
                    "World Render Commands: %3.3f ms",
                };
                struct { float r; float g; float b; } colors[8] = {
                    { 1.0, 0.0, 0.0 },
                    { 0.0, 1.0, 0.0 },
                    { 0.0, 0.0, 1.0 },
                    { 1.0, 1.0, 0.0 },
                    { 0.0, 1.0, 1.0 },
                    { 1.0, 0.0, 1.0 },
                    { 1.0, 1.0, 1.0 },
                    { 0.0, 0.0, 0.0 },
                };

                Graphics::Save();
                Graphics::Translate(infoPadding - 2.0, infoPadding, 0.0);
                Graphics::Scale(0.85, 0.85, 1.0);
                    snprintf(textBuffer, 256, "Frame Information");
                    DEBUG_DrawText(textBuffer, 0.0, 0.0);
                Graphics::Restore();

                Graphics::Save();
                Graphics::Translate(infoW - infoPadding - (8 * 16.0 * 0.85), infoPadding, 0.0);
                Graphics::Scale(0.85, 0.85, 1.0);
                    snprintf(textBuffer, 256, "FPS: %03.1f", FPS);
                    DEBUG_DrawText(textBuffer, 0.0, 0.0);
                Graphics::Restore();

                // Draw bar
                float total = 0.0001;
                for (int i = 0; i < typeCount; i++) {
                    if (types[i] < 0.0)
                        types[i] = 0.0;
                    total += types[i];
                }

                Graphics::Save();
                Graphics::Translate(infoPadding, 50.0, 0.0);
                    Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.25);
                    Graphics::FillRectangle(0.0, 0.0f, infoW - infoPadding * 2, 30.0);
                Graphics::Restore();

                float rectx = 0.0;
                for (int i = 0; i < typeCount; i++) {
                    Graphics::Save();
                    Graphics::Translate(infoPadding, 50.0, 0.0);
                        Graphics::SetBlendColor(colors[i].r, colors[i].g, colors[i].b, 0.5);
                        Graphics::FillRectangle(rectx, 0.0f, types[i] / total * (infoW - infoPadding * 2), 30.0);
                    Graphics::Restore();

                    rectx += types[i] / total * (infoW - infoPadding * 2);
                }

                // Draw list
                float listY = 90.0;
                infoPadding += infoPadding;
                for (int i = 0; i < typeCount; i++) {
                    Graphics::Save();
                    Graphics::Translate(infoPadding, listY, 0.0);
                        Graphics::SetBlendColor(colors[i].r, colors[i].g, colors[i].b, 0.5);
                        Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
                    Graphics::Scale(0.6, 0.6, 1.0);
                        snprintf(textBuffer, 256, typeNames[i], types[i]);
                        DEBUG_DrawText(textBuffer, 0.0, 0.0);
                        listY += 20.0;
                    Graphics::Restore();
                }

                float count = (float)Memory::MemoryUsage;
                const char* moniker = "B";

                if (count >= 1000000000) {
                    count /= 1000000000;
                    moniker = "GB";
                }
                else if (count >= 1000000) {
                    count /= 1000000;
                    moniker = "MB";
                }
                else if (count >= 1000) {
                    count /= 1000;
                    moniker = "KB";
                }

                listY += 30.0 - 20.0;

                Graphics::Save();
                Graphics::Translate(infoPadding / 2.0, listY, 0.0);
                Graphics::Scale(0.6, 0.6, 1.0);
                    snprintf(textBuffer, 256, "RAM Usage: %.3f %s", count, moniker);
                    DEBUG_DrawText(textBuffer, 0.0, 0.0);
                Graphics::Restore();
            Graphics::Restore();
        }

        MetricPresentTime = Clock::GetTicks();
        Graphics::Present();
        MetricPresentTime = Clock::GetTicks() - MetricPresentTime;

        // HACK: MacOS V-Sync timing gets disabled if window is not visible
        if (!Graphics::VsyncEnabled || Application::Platform == Platforms::MacOSX) {
            double clockEnd = Clock::End();
            double frameDurationRemainder = 1000.0 / TargetFPS - clockEnd;
            if (frameDurationRemainder >= 0.0) {
                double startTime = Clock::GetTicks();
                Clock::Delay(frameDurationRemainder - Overdelay);
                double delayTime = Clock::GetTicks() - startTime;
                if (delayTime > frameDurationRemainder)
                    Overdelay = delayTime - frameDurationRemainder;

                while ((Clock::GetTicks() - startTime) <= frameDurationRemainder);
            }
        }
        else {
            Clock::Delay(1);
        }

        BenchmarkFrameCount++;
        if (BenchmarkFrameCount == TargetFPS) {
            FPS = 1000.f / Clock::End() * TargetFPS;
            BenchmarkFrameCount = 0;
        }
    }

    Scene::Dispose();

    if (DEBUG_fontSprite) {
        DEBUG_fontSprite->Dispose();
        delete DEBUG_fontSprite;
        DEBUG_fontSprite = NULL;
    }

    Application::Cleanup();

    Memory::PrintLeak();
}

PUBLIC STATIC void Application::Cleanup() {
    ResourceManager::Dispose();
    AudioManager::Dispose();
    InputManager::Dispose();

    Graphics::Dispose();

    SDL_DestroyWindow(Application::Window);

    SDL_Quit();

    // Memory::PrintLeak();
}

PUBLIC STATIC void Application::LoadGameConfig() {
    StartingScene[0] = '\0';

    XMLNode* gameConfigXML = XMLParser::ParseFromResource("GameConfig.xml");
    if (!gameConfigXML) return;

    XMLNode* gameconfig = gameConfigXML->children[0];

    if (gameconfig->attributes.Exists("version"))
        XMLParser::MatchToken(gameconfig->attributes.Get("version"), "1.2");

    for (size_t i = 0; i < gameconfig->children.size(); i++) {
        XMLNode* configItem = gameconfig->children[i];
        if (XMLParser::MatchToken(configItem->name, "startscene")) {
            Token value = configItem->children[0]->name;
            memcpy(StartingScene, value.Start, value.Length);
            StartingScene[value.Length] = 0;
        }
    }

    XMLParser::Free(gameConfigXML);
}
PUBLIC STATIC void Application::LoadSettings() {
    if (Application::Settings)
        delete Application::Settings;

    Application::Settings = INI::Load("config.ini");
    // NOTE: If no settings could be loaded, create settings with default values.
    if (!Application::Settings) {
        Application::Settings = new INI;
        // Application::Settings->SetInteger("display", "width", Application::WIDTH * 2);
        // Application::Settings->SetInteger("display", "height", Application::HEIGHT * 2);
        Application::Settings->SetBool("display", "sharp", true);
        Application::Settings->SetBool("display", "crtFilter", true);
        Application::Settings->SetBool("display", "fullscreen", false);

        Application::Settings->SetInteger("input1", "up", 26);
        Application::Settings->SetInteger("input1", "down", 22);
        Application::Settings->SetInteger("input1", "left", 4);
        Application::Settings->SetInteger("input1", "right", 7);
        Application::Settings->SetInteger("input1", "confirm", 13);
        Application::Settings->SetInteger("input1", "deny", 14);
        Application::Settings->SetInteger("input1", "extra", 15);
        Application::Settings->SetInteger("input1", "pause", 19);
        Application::Settings->SetBool("input1", "vibration", true);

        Application::Settings->SetInteger("audio", "masterVolume", 100);
        Application::Settings->SetInteger("audio", "musicVolume", 100);
        Application::Settings->SetInteger("audio", "soundVolume", 100);
    }

    int LogLevel = 0;
    #ifdef DEBUG
    LogLevel = -1;
    #endif
    #ifdef ANDROID
    LogLevel = -1;
    #endif
    Application::Settings->GetInteger("dev", "logLevel", &LogLevel);
    Log::SetLogLevel(LogLevel);

    Application::Settings->GetBool("display", "vsync", &Graphics::VsyncEnabled);
    Application::Settings->GetInteger("display", "multisample", &Graphics::MultisamplingEnabled);

    int volume;
    Application::Settings->GetInteger("audio", "masterVolume", &volume);
    AudioManager::MasterVolume = volume / 100.0f;

    Application::Settings->GetInteger("audio", "musicVolume", &volume);
    AudioManager::MusicVolume = volume / 100.0f;

    Application::Settings->GetInteger("audio", "soundVolume", &volume);
    AudioManager::SoundVolume = volume / 100.0f;
}

PUBLIC STATIC int  Application::HandleAppEvents(void* data, SDL_Event* event) {
    switch (event->type) {
        case SDL_APP_TERMINATING:
            Scene::OnEvent(event->type);
            return 0;
        case SDL_APP_LOWMEMORY:
            Scene::OnEvent(event->type);
            return 0;
        case SDL_APP_WILLENTERBACKGROUND:
            Scene::OnEvent(event->type);
            return 0;
        case SDL_APP_DIDENTERBACKGROUND:
            Scene::OnEvent(event->type);
            return 0;
        case SDL_APP_WILLENTERFOREGROUND:
            Scene::OnEvent(event->type);
            return 0;
        case SDL_APP_DIDENTERFOREGROUND:
            Scene::OnEvent(event->type);
            return 0;
        default:
            return 1;
    }
}