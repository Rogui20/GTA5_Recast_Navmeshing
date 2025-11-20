/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Recast Navigation", "index.html", [
    [ "Introduction", "md_Docs_2__1__Introduction.html", [
      [ "What is a Navmesh and how does it work?", "md_Docs_2__1__Introduction.html#what-is-a-navmesh-and-how-does-it-work", [
        [ "Agent Attributes", "md_Docs_2__1__Introduction.html#agent-attributes", null ]
      ] ],
      [ "What is a Navmesh not?", "md_Docs_2__1__Introduction.html#what-is-a-navmesh-not", null ],
      [ "What is Recast Navigation?", "md_Docs_2__1__Introduction.html#what-is-recast-navigation", null ],
      [ "High-level overview of the Recast Navmesh-Building Process", "md_Docs_2__1__Introduction.html#high-level-overview-of-the-recast-navmesh-building-process", null ]
    ] ],
    [ "Building & Integrating", "md_Docs_2__2__BuildingAndIntegrating.html", [
      [ "Building RecastDemo", "md_Docs_2__2__BuildingAndIntegrating.html#building-recastdemo", [
        [ "Windows", "md_Docs_2__2__BuildingAndIntegrating.html#windows", null ],
        [ "macOS", "md_Docs_2__2__BuildingAndIntegrating.html#macos", null ],
        [ "Linux", "md_Docs_2__2__BuildingAndIntegrating.html#linux", null ]
      ] ],
      [ "Preprocessor Defines", "md_Docs_2__2__BuildingAndIntegrating.html#preprocessor-defines", null ],
      [ "Running Unit tests", "md_Docs_2__2__BuildingAndIntegrating.html#running-unit-tests", null ],
      [ "Integration", "md_Docs_2__2__BuildingAndIntegrating.html#integration", [
        [ "Source Integration", "md_Docs_2__2__BuildingAndIntegrating.html#source-integration", null ],
        [ "Installation through vcpkg", "md_Docs_2__2__BuildingAndIntegrating.html#installation-through-vcpkg", null ]
      ] ],
      [ "Customizing Allocation Behavior", "md_Docs_2__2__BuildingAndIntegrating.html#customizing-allocation-behavior", null ],
      [ "A Note on DLL exports and C API", "md_Docs_2__2__BuildingAndIntegrating.html#a-note-on-dll-exports-and-c-api", null ]
    ] ],
    [ "FAQ", "md_Docs_2__3__FAQ.html", [
      [ "Which C++ version and features do Recast use?", "md_Docs_2__3__FAQ.html#which-c-version-and-features-do-recast-use", null ],
      [ "What coordinate system and triangle winding order does Recast use?", "md_Docs_2__3__FAQ.html#what-coordinate-system-and-triangle-winding-order-does-recast-use", null ],
      [ "Why doesn't Recast use STL/Exceptions/RTTI/C++11/my favorite C++ feature?", "md_Docs_2__3__FAQ.html#why-doesnt-recast-use-stlexceptionsrttic11my-favorite-c-feature", null ],
      [ "How do I use Recast to build a navmesh?", "md_Docs_2__3__FAQ.html#how-do-i-use-recast-to-build-a-navmesh", null ],
      [ "How do Recast and Detour handle memory allocations?", "md_Docs_2__3__FAQ.html#how-do-recast-and-detour-handle-memory-allocations", null ],
      [ "Does Recast do any logging?", "md_Docs_2__3__FAQ.html#does-recast-do-any-logging", null ],
      [ "What are the dependencies for RecastDemo?", "md_Docs_2__3__FAQ.html#what-are-the-dependencies-for-recastdemo", null ]
    ] ],
    [ "Development Roadmap", "md_Docs_2__99__Roadmap.html", [
      [ "Short Term", "md_Docs_2__99__Roadmap.html#short-term", [
        [ "Documentation & Web Presence (WIP)", "md_Docs_2__99__Roadmap.html#documentation--web-presence-wip", null ],
        [ "More explicit variable names (WIP)", "md_Docs_2__99__Roadmap.html#more-explicit-variable-names-wip", null ],
        [ "Opt-in config value validation system", "md_Docs_2__99__Roadmap.html#opt-in-config-value-validation-system", null ]
      ] ],
      [ "Medium Term", "md_Docs_2__99__Roadmap.html#medium-term", [
        [ "STB-Style Single-Header Release Packaging", "md_Docs_2__99__Roadmap.html#stb-style-single-header-release-packaging", null ],
        [ "Ensure there's a good threading story", "md_Docs_2__99__Roadmap.html#ensure-theres-a-good-threading-story", null ],
        [ "More Tests", "md_Docs_2__99__Roadmap.html#more-tests", null ],
        [ "More POD structs for clarity in internals (WIP)", "md_Docs_2__99__Roadmap.html#more-pod-structs-for-clarity-in-internals-wip", null ],
        [ "Revisit structural organization", "md_Docs_2__99__Roadmap.html#revisit-structural-organization", null ]
      ] ],
      [ "Longer-Term", "md_Docs_2__99__Roadmap.html#longer-term", [
        [ "Higher-Level APIs", "md_Docs_2__99__Roadmap.html#higher-level-apis", null ],
        [ "C API", "md_Docs_2__99__Roadmap.html#c-api", null ]
      ] ],
      [ "New Recast/Detour Functionality", "md_Docs_2__99__Roadmap.html#new-recastdetour-functionality", [
        [ "Nav Volumes & 3D Navigation", "md_Docs_2__99__Roadmap.html#nav-volumes--3d-navigation", null ],
        [ "Climbing Markup & Navigation", "md_Docs_2__99__Roadmap.html#climbing-markup--navigation", null ],
        [ "Tooling", "md_Docs_2__99__Roadmap.html#tooling", null ],
        [ "More spatial querying abilities", "md_Docs_2__99__Roadmap.html#more-spatial-querying-abilities", null ],
        [ "Auto-markup system", "md_Docs_2__99__Roadmap.html#auto-markup-system", null ],
        [ "Formations, Group behaviors", "md_Docs_2__99__Roadmap.html#formations-group-behaviors", null ],
        [ "Vehicle Navigation & Movement", "md_Docs_2__99__Roadmap.html#vehicle-navigation--movement", null ],
        [ "Crowd Simulation and Flowfield movement systems", "md_Docs_2__99__Roadmap.html#crowd-simulation-and-flowfield-movement-systems", null ]
      ] ]
    ] ],
    [ "Sugestões para Organização dos Menus ImGui", "md_Docs_2ImGuiMenuOrganization.html", [
      [ "Estrutura geral", "md_Docs_2ImGuiMenuOrganization.html#estrutura-geral", null ],
      [ "Mesh Menu", "md_Docs_2ImGuiMenuOrganization.html#mesh-menu", null ],
      [ "Navmesh Menu", "md_Docs_2ImGuiMenuOrganization.html#navmesh-menu", null ],
      [ "Visibilidade e acessibilidade", "md_Docs_2ImGuiMenuOrganization.html#visibilidade-e-acessibilidade", null ],
      [ "Escalabilidade", "md_Docs_2ImGuiMenuOrganization.html#escalabilidade", null ],
      [ "Workflow sugerido", "md_Docs_2ImGuiMenuOrganization.html#workflow-sugerido", null ],
      [ "Ideias futuras", "md_Docs_2ImGuiMenuOrganization.html#ideias-futuras", null ]
    ] ],
    [ "Changelog", "md_CHANGELOG.html", [
      [ "<a href=\"https://github.com/recastnavigation/recastnavigation/compare/1.5.0...1.5.1\" >1.5.1</a> - 2016-02-22", "md_CHANGELOG.html#autotoc_md151httpsgithubcomrecastnavigationrecastnavigationcompare150151---2016-02-22", null ],
      [ "1.5.0 - 2016-01-24", "md_CHANGELOG.html#autotoc_md150---2016-01-24", null ],
      [ "1.4.0 - 2009-08-24", "md_CHANGELOG.html#autotoc_md140---2009-08-24", null ],
      [ "1.3.1 - 2009-07-24", "md_CHANGELOG.html#autotoc_md131---2009-07-24", null ],
      [ "1.3.1 - 2009-07-14", "md_CHANGELOG.html#autotoc_md131---2009-07-14", null ],
      [ "1.2.0 - 2009-06-17", "md_CHANGELOG.html#autotoc_md120---2009-06-17", null ],
      [ "1.1.0 - 2009-04-11", "md_CHANGELOG.html#autotoc_md110---2009-04-11", null ],
      [ "1.0.0 - 2009-03-29", "md_CHANGELOG.html#autotoc_md100---2009-03-29", null ]
    ] ],
    [ "Code of Conduct", "md_CODE__OF__CONDUCT.html", [
      [ "Our Pledge", "md_CODE__OF__CONDUCT.html#our-pledge", null ],
      [ "Our Standards", "md_CODE__OF__CONDUCT.html#our-standards", null ],
      [ "Enforcement Responsibilities", "md_CODE__OF__CONDUCT.html#enforcement-responsibilities", null ],
      [ "Scope", "md_CODE__OF__CONDUCT.html#scope", null ],
      [ "Enforcement", "md_CODE__OF__CONDUCT.html#enforcement", null ],
      [ "Enforcement Guidelines", "md_CODE__OF__CONDUCT.html#enforcement-guidelines", [
        [ "1. Correction", "md_CODE__OF__CONDUCT.html#autotoc_md1-correction", null ],
        [ "2. Warning", "md_CODE__OF__CONDUCT.html#autotoc_md2-warning", null ],
        [ "3. Temporary Ban", "md_CODE__OF__CONDUCT.html#autotoc_md3-temporary-ban", null ],
        [ "4. Permanent Ban", "md_CODE__OF__CONDUCT.html#autotoc_md4-permanent-ban", null ]
      ] ],
      [ "Attribution", "md_CODE__OF__CONDUCT.html#attribution", null ]
    ] ],
    [ "Contribution Guidelines", "md_CONTRIBUTING.html", [
      [ "Code of Conduct", "md_CONTRIBUTING.html#code-of-conduct-1", null ],
      [ "Have a Question or Problem?", "md_CONTRIBUTING.html#have-a-question-or-problem", null ],
      [ "Want a New Feature?", "md_CONTRIBUTING.html#want-a-new-feature", null ],
      [ "Found a Bug?", "md_CONTRIBUTING.html#found-a-bug", [
        [ "Submitting an Issue", "md_CONTRIBUTING.html#submitting-an-issue", null ],
        [ "Submitting a Pull Request", "md_CONTRIBUTING.html#submitting-a-pull-request", null ],
        [ "Commit Message Format", "md_CONTRIBUTING.html#commit-message-format", null ]
      ] ]
    ] ],
    [ "Contributing to SDL", "md_external_2SDL2_2docs_2CONTRIBUTING.html", [
      [ "Filing a GitHub issue", "md_external_2SDL2_2docs_2CONTRIBUTING.html#filing-a-github-issue", [
        [ "Reporting a bug", "md_external_2SDL2_2docs_2CONTRIBUTING.html#reporting-a-bug", null ],
        [ "Suggesting enhancements", "md_external_2SDL2_2docs_2CONTRIBUTING.html#suggesting-enhancements", null ]
      ] ],
      [ "Contributing code", "md_external_2SDL2_2docs_2CONTRIBUTING.html#contributing-code", [
        [ "Forking the project", "md_external_2SDL2_2docs_2CONTRIBUTING.html#forking-the-project", null ],
        [ "Following the style guide", "md_external_2SDL2_2docs_2CONTRIBUTING.html#following-the-style-guide", null ],
        [ "Running the tests", "md_external_2SDL2_2docs_2CONTRIBUTING.html#running-the-tests", null ],
        [ "Opening a pull request", "md_external_2SDL2_2docs_2CONTRIBUTING.html#opening-a-pull-request", null ]
      ] ],
      [ "Contributing to the documentation", "md_external_2SDL2_2docs_2CONTRIBUTING.html#contributing-to-the-documentation", [
        [ "Editing a function documentation", "md_external_2SDL2_2docs_2CONTRIBUTING.html#editing-a-function-documentation", null ],
        [ "Editing the wiki", "md_external_2SDL2_2docs_2CONTRIBUTING.html#editing-the-wiki", null ]
      ] ]
    ] ],
    [ "Android", "android.html", [
      [ "Requirements", "android.html#requirements", null ],
      [ "How the port works", "android.html#how-the-port-works", null ],
      [ "Building an app", "android.html#building-an-app", null ],
      [ "Customizing your application name", "android.html#customizing-your-application-name", null ],
      [ "Customizing your application icon", "android.html#customizing-your-application-icon", null ],
      [ "Loading assets", "android.html#loading-assets", null ],
      [ "Pause / Resume behaviour", "android.html#pause--resume-behaviour", null ],
      [ "Mouse / Touch events", "android.html#mouse--touch-events", null ],
      [ "Misc", "android.html#misc", null ],
      [ "Threads and the Java VM", "android.html#threads-and-the-java-vm", null ],
      [ "Using STL", "android.html#using-stl", null ],
      [ "Using the emulator", "android.html#using-the-emulator", null ],
      [ "Troubleshooting", "android.html#troubleshooting", null ],
      [ "Memory debugging", "android.html#memory-debugging", null ],
      [ "Graphics debugging", "android.html#graphics-debugging", null ],
      [ "Why is API level 19 the minimum required?", "android.html#why-is-api-level-19-the-minimum-required", null ],
      [ "A note regarding the use of the \"dirty rectangles\" rendering technique", "android.html#a-note-regarding-the-use-of-the-dirty-rectangles-rendering-technique", null ],
      [ "Ending your application", "android.html#ending-your-application", null ],
      [ "Known issues", "android.html#known-issues", null ]
    ] ],
    [ "CMake", "md_external_2SDL2_2docs_2README-cmake.html", [
      [ "Building SDL", "md_external_2SDL2_2docs_2README-cmake.html#building-sdl", null ],
      [ "Including SDL in your project", "md_external_2SDL2_2docs_2README-cmake.html#including-sdl-in-your-project", [
        [ "A system SDL library", "md_external_2SDL2_2docs_2README-cmake.html#a-system-sdl-library", null ],
        [ "Using a vendored SDL", "md_external_2SDL2_2docs_2README-cmake.html#using-a-vendored-sdl", null ]
      ] ],
      [ "CMake configuration options for platforms", "md_external_2SDL2_2docs_2README-cmake.html#cmake-configuration-options-for-platforms", [
        [ "iOS/tvOS", "md_external_2SDL2_2docs_2README-cmake.html#iostvos", [
          [ "Examples", "md_external_2SDL2_2docs_2README-cmake.html#examples", null ]
        ] ]
      ] ]
    ] ],
    [ "DirectFB", "directfb.html", [
      [ "Simple Window Manager", "directfb.html#simple-window-manager", null ],
      [ "OpenGL Support", "directfb.html#opengl-support", null ]
    ] ],
    [ "Dynamic API", "md_external_2SDL2_2docs_2README-dynapi.html", null ],
    [ "Emscripten", "md_external_2SDL2_2docs_2README-emscripten.html", [
      [ "The state of things", "md_external_2SDL2_2docs_2README-emscripten.html#the-state-of-things", null ],
      [ "RTFM", "md_external_2SDL2_2docs_2README-emscripten.html#rtfm", null ],
      [ "Porting your app to Emscripten", "md_external_2SDL2_2docs_2README-emscripten.html#porting-your-app-to-emscripten", null ],
      [ "Do you need threads?", "md_external_2SDL2_2docs_2README-emscripten.html#do-you-need-threads", null ],
      [ "Audio", "md_external_2SDL2_2docs_2README-emscripten.html#audio", null ],
      [ "Rendering", "md_external_2SDL2_2docs_2README-emscripten.html#rendering", null ],
      [ "Building SDL/emscripten", "md_external_2SDL2_2docs_2README-emscripten.html#building-sdlemscripten", null ],
      [ "Building your app", "md_external_2SDL2_2docs_2README-emscripten.html#building-your-app", null ],
      [ "Data files", "md_external_2SDL2_2docs_2README-emscripten.html#data-files", null ],
      [ "Debugging", "md_external_2SDL2_2docs_2README-emscripten.html#debugging", null ],
      [ "Questions?", "md_external_2SDL2_2docs_2README-emscripten.html#questions", null ]
    ] ],
    [ "GDK", "gdk.html", [
      [ "Requirements", "gdk.html#requirements-1", null ],
      [ "Windows GDK Status", "gdk.html#windows-gdk-status", null ],
      [ "VisualC-GDK Solution", "gdk.html#visualc-gdk-solution", null ],
      [ "Windows GDK Setup, Detailed Steps", "gdk.html#windows-gdk-setup-detailed-steps", [
        [ "1. Add a Gaming.Desktop.x64 Configuration", "gdk.html#autotoc_md1-add-a-gamingdesktopx64-configuration", null ],
        [ "2. Build SDL2 and SDL2main for GDK", "gdk.html#autotoc_md2-build-sdl2-and-sdl2main-for-gdk", null ],
        [ "3. Configuring Project Settings", "gdk.html#autotoc_md3-configuring-project-settings", null ],
        [ "4. Setting up SDL_main", "gdk.html#autotoc_md4-setting-up-sdl_main", null ],
        [ "5. Required DLLs", "gdk.html#autotoc_md5-required-dlls", null ],
        [ "6. Setting up MicrosoftGame.config", "gdk.html#autotoc_md6-setting-up-microsoftgameconfig", null ],
        [ "7. Adding Required Logos", "gdk.html#autotoc_md7-adding-required-logos", null ],
        [ "8. Copying any Data Files", "gdk.html#autotoc_md8-copying-any-data-files", null ],
        [ "9. Build and Run from Visual Studio", "gdk.html#autotoc_md9-build-and-run-from-visual-studio", null ],
        [ "10. Packaging and Installing Locally", "gdk.html#autotoc_md10-packaging-and-installing-locally", null ]
      ] ],
      [ "Xbox GDKX Setup", "gdk.html#xbox-gdkx-setup", null ],
      [ "Troubleshooting", "gdk.html#troubleshooting-1", null ]
    ] ],
    [ "Dollar Gestures", "dollar-gestures.html", [
      [ "Recording:", "dollar-gestures.html#recording", null ],
      [ "Performing:", "dollar-gestures.html#performing", null ],
      [ "Saving:", "dollar-gestures.html#saving", null ],
      [ "Loading:", "dollar-gestures.html#loading", null ],
      [ "Multi Gestures", "dollar-gestures.html#multi-gestures", null ],
      [ "Notes", "dollar-gestures.html#notes", null ]
    ] ],
    [ "git", "git.html", null ],
    [ "README-hg", "readme-hg.html", null ],
    [ "iOS", "ios.html", [
      [ "Building the Simple DirectMedia Layer for iOS 9.0+", "ios.html#building-the-simple-directmedia-layer-for-ios-90", null ],
      [ "Using the Simple DirectMedia Layer for iOS", "ios.html#using-the-simple-directmedia-layer-for-ios", null ],
      [ "Notes – Retina / High-DPI and window sizes", "ios.html#notes----retina--high-dpi-and-window-sizes", null ],
      [ "Notes – Application events", "ios.html#notes----application-events", null ],
      [ "Notes – Accelerometer as Joystick", "ios.html#notes----accelerometer-as-joystick", null ],
      [ "Notes – OpenGL ES", "ios.html#notes----opengl-es", null ],
      [ "Notes – Keyboard", "ios.html#notes----keyboard", null ],
      [ "Notes – Mouse", "ios.html#notes----mouse", null ],
      [ "Notes – Reading and Writing files", "ios.html#notes----reading-and-writing-files", null ],
      [ "Notes – xcFramework", "ios.html#notes----xcframework", null ],
      [ "Notes – iPhone SDL limitations", "ios.html#notes----iphone-sdl-limitations", null ],
      [ "Notes – CoreBluetooth.framework", "ios.html#notes----corebluetoothframework", null ],
      [ "Game Center", "ios.html#game-center", null ],
      [ "Deploying to older versions of iOS", "ios.html#deploying-to-older-versions-of-ios", null ]
    ] ],
    [ "KMSDRM on *BSD", "kmsdrm-on-bsd.html", [
      [ "SDL2 WSCONS input backend features", "kmsdrm-on-bsd.html#sdl2-wscons-input-backend-features", null ],
      [ "Partially working or no input on OpenBSD/NetBSD.", "kmsdrm-on-bsd.html#partially-working-or-no-input-on-openbsdnetbsd", null ],
      [ "Partially working or no input on FreeBSD.", "kmsdrm-on-bsd.html#partially-working-or-no-input-on-freebsd", null ]
    ] ],
    [ "Linux", "linux-1.html", [
      [ "Build Dependencies", "linux-1.html#build-dependencies", null ],
      [ "Joystick does not work", "linux-1.html#joystick-does-not-work", null ]
    ] ],
    [ "Mac OS X (aka macOS).", "md_external_2SDL2_2docs_2README-macos.html", [
      [ "Command Line Build", "md_external_2SDL2_2docs_2README-macos.html#command-line-build", null ],
      [ "Caveats for using SDL with Mac OS X", "md_external_2SDL2_2docs_2README-macos.html#caveats-for-using-sdl-with-mac-os-x", null ],
      [ "Using the Simple DirectMedia Layer with a traditional Makefile", "md_external_2SDL2_2docs_2README-macos.html#using-the-simple-directmedia-layer-with-a-traditional-makefile", null ],
      [ "Using the Simple DirectMedia Layer with Xcode", "md_external_2SDL2_2docs_2README-macos.html#using-the-simple-directmedia-layer-with-xcode", [
        [ "First steps", "md_external_2SDL2_2docs_2README-macos.html#first-steps", null ],
        [ "Building the Framework", "md_external_2SDL2_2docs_2README-macos.html#building-the-framework", null ],
        [ "Build Options", "md_external_2SDL2_2docs_2README-macos.html#build-options", null ],
        [ "Building the Testers", "md_external_2SDL2_2docs_2README-macos.html#building-the-testers", null ],
        [ "Using the Project Stationary", "md_external_2SDL2_2docs_2README-macos.html#using-the-project-stationary", null ],
        [ "Setting up a new project by hand", "md_external_2SDL2_2docs_2README-macos.html#setting-up-a-new-project-by-hand", null ],
        [ "Building from command line", "md_external_2SDL2_2docs_2README-macos.html#building-from-command-line", null ],
        [ "Running your app", "md_external_2SDL2_2docs_2README-macos.html#running-your-app", null ]
      ] ],
      [ "Implementation Notes", "md_external_2SDL2_2docs_2README-macos.html#implementation-notes", [
        [ "Working directory", "md_external_2SDL2_2docs_2README-macos.html#working-directory", null ],
        [ "You have a Cocoa App!", "md_external_2SDL2_2docs_2README-macos.html#you-have-a-cocoa-app", null ]
      ] ],
      [ "Bug reports", "md_external_2SDL2_2docs_2README-macos.html#bug-reports", null ]
    ] ],
    [ "Nintendo 3DS", "md_external_2SDL2_2docs_2README-n3ds.html", [
      [ "Building", "md_external_2SDL2_2docs_2README-n3ds.html#building", null ],
      [ "Notes", "md_external_2SDL2_2docs_2README-n3ds.html#notes-1", null ]
    ] ],
    [ "Native Client", "native-client.html", [
      [ "Building SDL for NaCl", "native-client.html#building-sdl-for-nacl", null ],
      [ "Running tests", "native-client.html#running-tests", null ],
      [ "RWops and nacl_io", "native-client.html#rwops-and-nacl_io", null ],
      [ "TODO - Known Issues", "native-client.html#todo---known-issues", null ]
    ] ],
    [ "Nokia N-Gage", "nokia-n-gage.html", [
      [ "Compiling", "nokia-n-gage.html#compiling", null ],
      [ "Current level of implementation", "nokia-n-gage.html#current-level-of-implementation", null ],
      [ "Acknowledgements", "nokia-n-gage.html#acknowledgements", null ]
    ] ],
    [ "Simple DirectMedia Layer 2 for OS/2 & eComStation", "simple-directmedia-layer-2-for-os2--ecomstation.html", [
      [ "Compiling:", "simple-directmedia-layer-2-for-os2--ecomstation.html#compiling-1", null ],
      [ "Installing:", "simple-directmedia-layer-2-for-os2--ecomstation.html#installing", null ],
      [ "Joysticks in SDL2:", "simple-directmedia-layer-2-for-os2--ecomstation.html#joysticks-in-sdl2", null ]
    ] ],
    [ "Pandora", "pandora.html", null ],
    [ "Platforms", "platforms.html", null ],
    [ "Porting", "porting.html", null ],
    [ "PS2", "ps2.html", [
      [ "Building", "ps2.html#building-1", null ],
      [ "Hints", "ps2.html#hints", null ],
      [ "Notes", "ps2.html#notes-2", null ],
      [ "Getting PS2 Dev", "ps2.html#getting-ps2-dev", null ],
      [ "Running on PCSX2 Emulator", "ps2.html#running-on-pcsx2-emulator", null ],
      [ "To Do", "ps2.html#to-do", null ]
    ] ],
    [ "PSP", "psp.html", [
      [ "Building", "psp.html#building-2", null ],
      [ "Getting PSP Dev", "psp.html#getting-psp-dev", null ],
      [ "Running on PPSSPP Emulator", "psp.html#running-on-ppsspp-emulator", null ],
      [ "Compiling a HelloWorld", "psp.html#compiling-a-helloworld", null ],
      [ "To Do", "psp.html#to-do-1", null ]
    ] ],
    [ "Raspberry Pi", "raspberry-pi.html", [
      [ "Features", "raspberry-pi.html#features", null ],
      [ "Raspbian Build Dependencies", "raspberry-pi.html#raspbian-build-dependencies", null ],
      [ "NEON", "raspberry-pi.html#neon", null ],
      [ "Cross compiling from x86 Linux", "raspberry-pi.html#cross-compiling-from-x86-linux", null ],
      [ "Apps don't work or poor video/audio performance", "raspberry-pi.html#apps-dont-work-or-poor-videoaudio-performance", null ],
      [ "No input", "raspberry-pi.html#no-input", null ],
      [ "No HDMI Audio", "raspberry-pi.html#no-hdmi-audio", null ],
      [ "Text Input API support", "raspberry-pi.html#text-input-api-support", null ],
      [ "OpenGL problems", "raspberry-pi.html#opengl-problems", null ],
      [ "Notes", "raspberry-pi.html#notes-3", null ]
    ] ],
    [ "RISC OS", "risc-os.html", [
      [ "Compiling:", "risc-os.html#compiling-2", null ],
      [ "Current level of implementation", "risc-os.html#current-level-of-implementation-1", null ]
    ] ],
    [ "Touch", "touch.html", [
      [ "System Specific Notes", "touch.html#system-specific-notes", null ],
      [ "Events", "touch.html#events", null ],
      [ "Functions", "touch.html#functions", null ],
      [ "Notes", "touch.html#notes-4", null ]
    ] ],
    [ "Versioning", "md_external_2SDL2_2docs_2README-versions.html", [
      [ "Since 2.23.0", "md_external_2SDL2_2docs_2README-versions.html#since-2230", null ],
      [ "Before 2.23.0", "md_external_2SDL2_2docs_2README-versions.html#before-2230", null ]
    ] ],
    [ "Using SDL with Microsoft Visual C++", "using-sdl-with-microsoft-visual-c.html", null ],
    [ "PS Vita", "ps-vita.html", [
      [ "Building", "ps-vita.html#building-3", null ],
      [ "Notes", "ps-vita.html#notes-5", null ]
    ] ],
    [ "WinCE", "wince.html", null ],
    [ "Windows", "md_external_2SDL2_2docs_2README-windows.html", [
      [ "LLVM and Intel C++ compiler support", "md_external_2SDL2_2docs_2README-windows.html#llvm-and-intel-c-compiler-support", null ],
      [ "OpenGL ES 2.x support", "md_external_2SDL2_2docs_2README-windows.html#opengl-es-2x-support", null ],
      [ "Vulkan Surface Support", "md_external_2SDL2_2docs_2README-windows.html#vulkan-surface-support", null ]
    ] ],
    [ "WinRT", "winrt.html", [
      [ "Requirements", "winrt.html#requirements-2", null ],
      [ "Status", "winrt.html#status", null ],
      [ "Upgrade Notes", "winrt.html#upgrade-notes", null ],
      [ "Setup, High-Level Steps", "winrt.html#setup-high-level-steps", null ],
      [ "Setup, Detailed Steps", "winrt.html#setup-detailed-steps", [
        [ "1. Create a new project", "winrt.html#autotoc_md1-create-a-new-project", null ],
        [ "2. Remove unneeded files from the project", "winrt.html#autotoc_md2-remove-unneeded-files-from-the-project", null ],
        [ "3. Add references to SDL's project files", "winrt.html#autotoc_md3-add-references-to-sdls-project-files", null ],
        [ "4. Adjust Your App's Build Settings", "winrt.html#autotoc_md4-adjust-your-apps-build-settings", null ],
        [ "5. Add a WinRT-appropriate main function, and a blank-cursor image, to the app.", "winrt.html#autotoc_md5-add-a-winrt-appropriate-main-function-and-a-blank-cursor-image-to-the-app", null ],
        [ "6. Add app code and assets", "winrt.html#autotoc_md6-add-app-code-and-assets", [
          [ "6.A. ... when creating a new app", "winrt.html#autotoc_md6a--when-creating-a-new-app", null ],
          [ "6.B. Adding code and assets", "winrt.html#autotoc_md6b-adding-code-and-assets", null ]
        ] ],
        [ "7. Build and run your app", "winrt.html#autotoc_md7-build-and-run-your-app", [
          [ "7.A. Running apps on older, ARM-based, \"Windows RT\" devices", "winrt.html#autotoc_md7a-running-apps-on-older-arm-based-windows-rt-devices", null ]
        ] ]
      ] ],
      [ "Troubleshooting", "winrt.html#troubleshooting-2", null ]
    ] ],
    [ "Simple DirectMedia Layer", "md_external_2SDL2_2docs_2README.html", null ],
    [ "Release checklist", "md_external_2SDL2_2docs_2release__checklist.html", [
      [ "New feature release", "md_external_2SDL2_2docs_2release__checklist.html#new-feature-release", null ],
      [ "New bugfix release", "md_external_2SDL2_2docs_2release__checklist.html#new-bugfix-release", null ],
      [ "After a feature release", "md_external_2SDL2_2docs_2release__checklist.html#after-a-feature-release", null ],
      [ "New development prerelease", "md_external_2SDL2_2docs_2release__checklist.html#new-development-prerelease", null ]
    ] ],
    [ "Simple DirectMedia Layer (SDL) Version 2.0", "md_external_2SDL2_2README.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", "globals_vars" ],
        [ "Typedefs", "globals_type.html", "globals_type" ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", "globals_eval" ],
        [ "Macros", "globals_defs.html", "globals_defs" ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"ConvexVolumeTool_8cpp.html",
"DetourNavMeshBuilder_8cpp.html#a1888720290d8beeb3162f90e197ff7b3",
"GtaNavAPI_8h.html#aad1c0bdb748f6c59de98270787f776b5",
"GtaNavViewer_2imgui_2imgui_8cpp.html#a3d7cc9d0165dd6653b8a4b99a6e2b9e9",
"GtaNavViewer_2imgui_2imgui_8h.html#a0512146617bb55e24ebcfbe3ce6553d5",
"GtaNavViewer_2imgui_2imgui_8h.html#a3685a04624f724222fa78824de3a1c63",
"GtaNavViewer_2imgui_2imgui_8h.html#a665f6b404eea9a640b1222a87236fe19",
"GtaNavViewer_2imgui_2imgui_8h.html#aa22ffe36b188427d712447ec465203d4a07776ca24bec3520f4c4e787ec62ee3f",
"GtaNavViewer_2imgui_2imgui_8h.html#abbc1d650f5c3ffe6af55ee82a491fa6aa20ae5d115100728aa8ac56938d7b6e0e",
"GtaNavViewer_2imgui_2imgui_8h.html#aef500c7434555b6cd61e14e3603c2cdaa66771ec6a67f746973253643de7ce86b",
"RecastDemo_2Include_2ChunkyTriMesh_8h_source.html",
"RecastRegion_8cpp.html#a5b2ece2d7931891cdb3fe0b7b41de1b4",
"SDL__audio_8h.html#ae3667d6ba706a3ca7affd8611632fa2c",
"SDL__config__emscripten_8h.html#a8cca22fcb89eab12235619bbbe75be26",
"SDL__config__macosx_8h.html#a6f6c666b70354db02cd6d26318ae70d2",
"SDL__config__os2_8h.html#af5e1c674bb7ab42ee349e48b33705a04",
"SDL__config__winrt_8h.html#a7ff023a4e451532aa34a6dace7f6255e",
"SDL__events_8h.html#a7147e3df7940e4e43a997dc99e0611eb",
"SDL__hidapi_8h.html#a6f20cc83879d21bad4306f540475dd8f",
"SDL__joystick_8h.html#a2215030cf283295569fee41fd0251e01",
"SDL__keycode_8h.html#a179ce01fa41d35408f06b4b3d1cd9d3da9fc776881141300074b15b5caea44d59",
"SDL__mouse_8h.html#a9500bda83bcc0e9afee1438a79019c5f",
"SDL__opengl_8h.html#a2260e752d41d9c53dacfc9f3bb0292fb",
"SDL__opengl_8h.html#a555996a28d3dee6168b4647f1b7225b1",
"SDL__opengl_8h.html#a86c0429fe9f77fd85af5b0be64a6cd1f",
"SDL__opengl_8h.html#ab82a54c12a829fc926927616b64ca8d7",
"SDL__opengl_8h.html#aec1edb7e90893af481f33bb1c8865c52",
"SDL__opengl__glext_8h.html#a07073f93b12518efd0ee94e773dd911a",
"SDL__opengl__glext_8h.html#a127048bfb84d994717266e27d9ae59e8",
"SDL__opengl__glext_8h.html#a1c169657d62b7195accca1e9199d906c",
"SDL__opengl__glext_8h.html#a27136d21de8f3e0b2ec99af56af5003f",
"SDL__opengl__glext_8h.html#a322882ac80bc474854b21cdd99cd6a3f",
"SDL__opengl__glext_8h.html#a3dac2f67195ebd4e6b711b6e742b3056",
"SDL__opengl__glext_8h.html#a49365a88e4ed0c4c2d0cee5bc9982b0c",
"SDL__opengl__glext_8h.html#a54cf67ce090dea19e080d2d27e90ed7e",
"SDL__opengl__glext_8h.html#a6081beecc6dc9b99f3b321e029c4757e",
"SDL__opengl__glext_8h.html#a6b9ae31edbc4be3478eb7261e83c3f69",
"SDL__opengl__glext_8h.html#a769c6fa1de10e2a6f3149fd8c7620157",
"SDL__opengl__glext_8h.html#a81d86c45f362a6e0b11779d7cb45f227",
"SDL__opengl__glext_8h.html#a8ce72fc36283dde31b254b6d6b932c0d",
"SDL__opengl__glext_8h.html#a98d07dc804eb9aae3a8819cf03942ffa",
"SDL__opengl__glext_8h.html#aa4f194e5160ccbb5a8e83ab6ef4676d2",
"SDL__opengl__glext_8h.html#ab0cc9bbbb92b1d82eda28547d659ffb8",
"SDL__opengl__glext_8h.html#aba62d0bf6d4cf702d57410e8cb000185",
"SDL__opengl__glext_8h.html#ac50c5be562a78fb8f161c7e644f64338",
"SDL__opengl__glext_8h.html#ad0460ad4142bc7fc6e24f2f8afb5050b",
"SDL__opengl__glext_8h.html#adc5ddc92dc3e28fa678a7d648fda123d",
"SDL__opengl__glext_8h.html#ae64453e3222c8b6d37dec88492552ae4",
"SDL__opengl__glext_8h.html#af14f29ab9830a811949fc6e34898f219",
"SDL__opengl__glext_8h.html#afca1b2c3a11b046b45e36fc15b4c47a2",
"SDL__opengles2__gl2_8h.html#a493a344af6533267a8be2b58a23130fa",
"SDL__opengles2__gl2_8h.html#ac2a90866d1cde7e6180739930c102f6e",
"SDL__opengles2__gl2ext_8h.html#a0fec65512c27048437b49a077f158d71",
"SDL__opengles2__gl2ext_8h.html#a2c7bfa9f25fda620ae366e4243f39593",
"SDL__opengles2__gl2ext_8h.html#a4e5a4637019eb778a4a1cdc54ddb3809",
"SDL__opengles2__gl2ext_8h.html#a6c8c6cd482cd2a24af4ef13dbe47f536",
"SDL__opengles2__gl2ext_8h.html#a8b51f8addfef310a7487567243645de5",
"SDL__opengles2__gl2ext_8h.html#aa9d286e72c034768998fe7a07d5f5e1a",
"SDL__opengles2__gl2ext_8h.html#ac5050f65d8a6dd4eb12303df7a15ae01",
"SDL__opengles2__gl2ext_8h.html#ae6523ac0c133854dab6135ee874fc20b",
"SDL__pixels_8h.html#a0a65fc19d5122dd3c544f283553ad42c",
"SDL__render_8h.html#aa52b062e271226c5ec8cdb8aa6dc69eb",
"SDL__scancode_8h.html#a82ab7cff701034fb40a47b5b3a02777babb372eb6f2e96fcfb271210326f1fbc0",
"SDL__stdinc_8h.html#a92eeaa95348c308ebd882ca85eccd684",
"SDL__test__fuzzer_8h.html#a2a0e63a6e02a55141edbdfa53b201c9b",
"SDL__video_8h.html#a68eb0d850934fa1c7ac12c30f2d56a84",
"android.html#troubleshooting",
"classOffMeshConnectionTool.html#af023858b0abb68e538a842ddc9e25022",
"classTempObstacleCreateTool.html",
"classrcScopedTimer.html#a3e1e6f826e8081e67f23cd6c35653b6e",
"functions_func_n.html",
"glad_8h.html#a1985184535af747c30a8a12481e2ffd9",
"glad_8h.html#a3f092d57aaf9817fbdd2b51a650992b4",
"glad_8h.html#a619bc20e8198de3bd3f3d7fc34de66b2",
"glad_8h.html#a854ad67c431d89c86040972cc79e35d6",
"glad_8h.html#aa88a58126327afa56d66dd063a6f1bbe",
"glad_8h.html#acb5d61320a19542050d9df3b056fb31c",
"glad_8h.html#af17aef68eaddfa6109474d953fc65888",
"imguiRenderGL_8cpp.html#a413aaddb1ddea117f86218f2ca0234fe",
"imgui__impl__allegro5_8cpp.html#a51b5f14a5b59eacde20aa19091f3d253",
"imgui__impl__glfw_8h.html#ab72d9035883720a797bc3cb7e3e8a482",
"imgui__impl__opengl3__loader_8h.html#a80a33c79f69417372d65d2a65ca36d49",
"imgui__impl__sdl3_8cpp.html#abb11ffd5559f0d6b9d717db8fb7b4ec2",
"imgui__impl__win32_8cpp.html#a29696f9c6ffa4798de8b774bbff5baca",
"imgui__internal_8h.html#a2fc7faf4aea2e95ce95eda702800fde5",
"imgui__internal_8h.html#a6808a631d32de769e33eaac85ff3d85c",
"imgui__internal_8h.html#a9aa9a370e4d66959398a0833db4a7508",
"imgui__internal_8h.html#ad3fdd202d1973cfeb10564b8f55b177ca097bd55eceaa4c0e4ef88f6aee5f38c0",
"imgui__widgets_8cpp.html#a4d14a9af02c8bc14a1a8a9e6fda9bfa2",
"md_CONTRIBUTING.html#have-a-question-or-problem",
"structExampleAppConsole.html#a01c861ed21b9ec8de5e1ee2c762c627a",
"structImDrawList.html#a405377158f0028ad8b4fb6509eef4532",
"structImFontAtlasRect.html#aa0baf4bb95c9940b2adede3a68fb7b82",
"structImGuiContext.html#a5e31703c42698a572be557ec57c5cbe7",
"structImGuiDemoWindowData.html#a29a2cb8a85e2e1091a51b707e23e1678",
"structImGuiIO.html#a9d3bbbd43311c00ccd94a3e497a68631",
"structImGuiMultiSelectIO.html",
"structImGuiSettingsHandler.html#a461105eb071f511b480695f9915c821b",
"structImGuiTable.html#a79fe77ae5bb4b1280fcfbbd2e98ad882",
"structImGuiViewport.html#ac4c1ce7d912b72b42a14f00c8cefd8d0",
"structImGuiWindowTempData.html#af86ba9662f7aeb95c49e785d941f4c69",
"structImGui__ImplSDL3__Data.html#a94b82b384a363200ebdfbb12b212291e",
"structImRect.html#a848039e6c07771d4ae76786c58c02f64",
"structNavMeshContext.html#a915336fe9ab5534f5f680dc988ddf494",
"structSDL__ControllerSensorEvent.html#aac19e4034212a30d64066a122d5db2a8",
"structSDL__MouseWheelEvent.html#ab45eb1895217214ecb773fc555e08f6c",
"structSampleToolState.html#ab9dda86ad5ef7d23b6da69dbbc5ded1e",
"structduFileIO.html#ab436e18b116c61875dbdf5faafda9694",
"structstbtt__vertex.html#a68227d28643f5667064fa3c385f4ea7d"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';