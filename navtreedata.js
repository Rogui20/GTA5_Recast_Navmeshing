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
"GtaNavAPI_8cpp.html#adb96a12177dd013ebb620aba3d990ba6",
"GtaNavViewer_2imgui_2imgui_8cpp.html#a338deb537666a886cf3ece08c07e9fa9",
"GtaNavViewer_2imgui_2imgui_8h.html#a0470693929ed9930cbf5b737577c2414",
"GtaNavViewer_2imgui_2imgui_8h.html#a35f4d8837c0fc776f3fbe41c07716301a50bea77c6a5da0fecfe58c8a948fa6cd",
"GtaNavViewer_2imgui_2imgui_8h.html#a650093c450bfc0161d09c81cf08270cead5a0cf27d7ead5434b9eb02d06cc8b19",
"GtaNavViewer_2imgui_2imgui_8h.html#aa22ffe36b188427d712447ec465203d4",
"GtaNavViewer_2imgui_2imgui_8h.html#abafef4bb3e3654efd96a47b2e22870a7",
"GtaNavViewer_2imgui_2imgui_8h.html#aef500c7434555b6cd61e14e3603c2cdaa3adaf9ce80f7265597749e1e8eb17b6b",
"RecastDebugDraw_8h.html",
"RecastMesh_8cpp.html#adbc7fb647489ff66ceda06839a1c4487",
"SDL__audio_8h.html#aa68e60a33666ff4f2d36f3827b1d6061",
"SDL__config__emscripten_8h.html#a550e5c272cc3cf3814651721167dcd23",
"SDL__config__macosx_8h.html#a494d1ecf5a4567ccbf7901e3be337bc7",
"SDL__config__os2_8h.html#ac5f71bfd69698a6db1ae54177fcac7ea",
"SDL__config__winrt_8h.html#a3f7e8b5e13d96d1de1a0848f7aa80157",
"SDL__events_8h.html#a3b589e89be6b35c02e0dd34a55f3fccaadb3154654baf003910e75d74b507a82b",
"SDL__haptic_8h.html#ae2185c0814f107dd5851dd743e9ea71c",
"SDL__joystick_8h.html",
"SDL__keycode_8h.html#a179ce01fa41d35408f06b4b3d1cd9d3da876db2a58663a107a6f9355b48be119d",
"SDL__mouse_8h.html#a2f2259cdf30272b3c4cc1e3236cb8071",
"SDL__opengl_8h.html#a1d7a9f46a0e0ac25b120a626529b8b89",
"SDL__opengl_8h.html#a50306f6feac54b9d3bf972272df48de0",
"SDL__opengl_8h.html#a8287eef1d8138f5cd1cd7cce1bc8e8b2",
"SDL__opengl_8h.html#ab3f8ed1ddc1cc1b2889325c9b7bd55e3",
"SDL__opengl_8h.html#ae8636a75e6d389ef8145d46764a9bb4b",
"SDL__opengl__glext_8h.html#a0605d5ef41613665b34391f9ba398997",
"SDL__opengl__glext_8h.html#a117a77e99f3fa4a9dea4351292641126",
"SDL__opengl__glext_8h.html#a1b36c3bc2552573501221c294e71fac8",
"SDL__opengl__glext_8h.html#a2634d5ad870f65f42207471f33d6f025",
"SDL__opengl__glext_8h.html#a312e8d1ce91a7c55157b545ee0b47d2a",
"SDL__opengl__glext_8h.html#a3c4c37566d7aa532e57ed5e77153c7d9",
"SDL__opengl__glext_8h.html#a4845f1431f0325168a4c8f36d8d80c16",
"SDL__opengl__glext_8h.html#a540318e98b09af62428147cceff01d37",
"SDL__opengl__glext_8h.html#a5f35011157007ff9d591444edc4da526",
"SDL__opengl__glext_8h.html#a6aaf6b2864c97ce8697859cd24452259",
"SDL__opengl__glext_8h.html#a75fbfcd1a5c5f71baa7d8e67e12505a5",
"SDL__opengl__glext_8h.html#a80fbaabf2e74fd6106c285fb3e3220d2",
"SDL__opengl__glext_8h.html#a8c428088ab256025d07b8449d36f73c6",
"SDL__opengl__glext_8h.html#a97d4815b832e756dc3841617c5f4540e",
"SDL__opengl__glext_8h.html#aa40ceda988bbf83ecaa34c6359505bf5",
"SDL__opengl__glext_8h.html#aafd3fea47e8744b58cac668de72ae75d",
"SDL__opengl__glext_8h.html#ab9964f4ef836cd122d7771e69a32fdce",
"SDL__opengl__glext_8h.html#ac429f0ea5d9d2f9ea349cf825b4cb769",
"SDL__opengl__glext_8h.html#acf59ffd6f578724be844b8167ac611bd",
"SDL__opengl__glext_8h.html#adb7e4de56af0a02b6d9792a286839194",
"SDL__opengl__glext_8h.html#ae545debabe0f0f4d3ca9331981dbaf6b",
"SDL__opengl__glext_8h.html#af026b485d7b4110f7ce152362d475598",
"SDL__opengl__glext_8h.html#afb91ddece6caa6f4df3fbe33eac11cdc",
"SDL__opengles2__gl2_8h.html#a3f1e1951ac224b0f42929facb17e983f",
"SDL__opengles2__gl2_8h.html#ab96e858462d995e125a54e2021c98e28",
"SDL__opengles2__gl2ext_8h.html#a0d7ae2b87ce4952363f239dbe56df7a6",
"SDL__opengles2__gl2ext_8h.html#a2a1126bad7f9ec5195195be6fc46fe19",
"SDL__opengles2__gl2ext_8h.html#a4a05d456bbd020fc04bc0b0f2291452c",
"SDL__opengles2__gl2ext_8h.html#a6a4be3283d87c01a23d2f12fa4c7a6b1",
"SDL__opengles2__gl2ext_8h.html#a88f3e23a53299bc16040a782cc3116d1",
"SDL__opengles2__gl2ext_8h.html#aa6a007e2f87a5a45ea1a51d79e930a11",
"SDL__opengles2__gl2ext_8h.html#ac237206a57067d2eac8bb17d2103d15b",
"SDL__opengles2__gl2ext_8h.html#ae29a6d468158c657dfae790a9db0fcb9",
"SDL__opengles2__khrplatform_8h.html#a55dc5b71f5c76458b3bfaa7621520796",
"SDL__render_8h.html#a6c60ba0582abbdf342e1d305e3c52362",
"SDL__scancode_8h.html#a82ab7cff701034fb40a47b5b3a02777baa7d4a456411ad010c8c330e49f3fa4e4",
"SDL__stdinc_8h.html#a7b721b0e22698c8eb6efc5341003b582",
"SDL__test__crc32_8h.html#a8f6936196be4a985dadbb0c0549aacf2",
"SDL__video_8h.html#a42d2451050ae01aaf2974344b9d4a812",
"ViewerAppUtils_8cpp.html",
"classNavMeshData.html#a135adc4c19be0872f55861a9950f9396",
"classSample__TempObstacles.html#a6ab371394b122966ab5de8619bd8646aad240eab884b6fce214ae17ee58ff1fc5",
"classdtTileCache.html#a19518556c937306a2fb6ee71bbf5bcd4",
"external_2glm_2simd_2platform_8h.html#a8df3e93949a3af2e674f11cd53c13b6c",
"glad_8h.html#a0c5465806167aaa659a8dac3169c17a1",
"glad_8h.html#a316d4feb613d04854aba437a278add28",
"glad_8h.html#a5489df792b3263170eea381093ee6111",
"glad_8h.html#a7a15ed22661a401ae625e517aea781b1",
"glad_8h.html#a9ce229fc8d6a6113a9892d6b98ae1e70",
"glad_8h.html#ac0414c8e4f5b6d86b75e466b1388b87b",
"glad_8h.html#ae4f8384133dc7f9a68ffde907193ca68",
"globals_func_o.html",
"imgui__draw_8cpp.html#a4df703825e073b91ea24956d6021ee59",
"imgui__impl__dx9_8h.html#a22379034d32f262395c35ceace41e921",
"imgui__impl__opengl3__loader_8h.html#a173a2b2453322dd8d613df6cf2e72480",
"imgui__impl__sdl2_8cpp.html#a34f0127746f48fbac6ae14fdda8ccc33",
"imgui__impl__vulkan_8cpp.html#ad59d9ae32878d956baa426146fa8a49b",
"imgui__internal_8h.html#a159f9360f0ece575f2cef41f8df4dca0",
"imgui__internal_8h.html#a51e29bae6ffe6afa11810a05d8aef616",
"imgui__internal_8h.html#a823c5e9f84cec3d4e2a66abd378829f0a26b9c169a1b04776d9fb55e8379451bc",
"imgui__internal_8h.html#abfdf313fa0c69cf4ab215cbd0b157f7b",
"imgui__internal_8h.html#af519e6f7dfbfeabee65f22dd081f45ea",
"imstb__truetype_8h.html#a9f906556cc09c60d136cb9af18305ef3",
"ps2.html#running-on-pcsx2-emulator",
"structGuiState.html#adbb0ec177af2c0beaf4471339952d522",
"structImFontAtlas.html#a1fd42ec0b657ff4a1447290894c331de",
"structImGuiContext.html#a0f175fa6d802c77e1c3bda0555bffaef",
"structImGuiContext.html#ad2d2880c154c0528b0ef338576d0f8e2",
"structImGuiGroupData.html#a68860f61c4e6c0b3204a21357dc4608f",
"structImGuiKeyData.html",
"structImGuiPlatformIO.html#a5bb0721dd7e75c05afeb818b9541de6b",
"structImGuiTabBar.html#a616acfd5a38deb934c5a079b458ca5a9",
"structImGuiTableTempData.html",
"structImGuiWindow.html#acd2909ae3b7079de81d528327cc11dfd",
"structImGui__ImplDX9__ViewportData.html#a642f72619785c54e1c33751adbd2785f",
"structImGui__ImplVulkan__ViewportData.html#a511f13bcf07b4ead4c8c4dd5dfe005ff",
"structImVec2ih.html#af8489174701c9070f3874cce8c1e514f",
"structSDLTest__CommonState.html#aff43a763af98b8eb651c41ec931e5d93",
"structSDL__JoyAxisEvent.html#a60ecfc70df9eeef8e33b5dd7dc060389",
"structSDL__UserEvent.html#ab2893a12be2f97195f16463a23107913",
"structdtPolyState.html#a8e3a8a1c15338bbe761ef4d9ca2b4949",
"structstbrp__context.html#a007509feee322404083034e4c2d3dc5d"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';