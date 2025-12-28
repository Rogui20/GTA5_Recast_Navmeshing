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
      [ "🚀 Features", "index.html#autotoc_md-features", null ],
      [ "⚡ Getting Started", "index.html#autotoc_md-getting-started", null ],
      [ "⚙ How it Works", "index.html#autotoc_md-how-it-works", null ],
      [ "📚 Documentation & Links", "index.html#autotoc_md-documentation--links", null ],
      [ "❤ Community", "index.html#autotoc_md-community", null ],
      [ "⚖ License", "index.html#autotoc_md-license", null ],
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
"ExternC_8h.html#a243a7a0e45dc62290d8bcd691c8b8744",
"GtaNavViewer_2imgui_2imgui_8cpp.html#a0e50a00683cc7e938d41873d0d6e233c",
"GtaNavViewer_2imgui_2imgui_8cpp.html#aeb054c43ad8aae45de24efc759a0ff7a",
"GtaNavViewer_2imgui_2imgui_8h.html#a2eb1181cc1d7872a061df8731141dde9a025f083574198762f5a7337ef2aaffef",
"GtaNavViewer_2imgui_2imgui_8h.html#a650093c450bfc0161d09c81cf08270cea076a70358313d3ce63c8b46e48899bc8",
"GtaNavViewer_2imgui_2imgui_8h.html#a91442880bed105ca5fe1be683b91d9a0",
"GtaNavViewer_2imgui_2imgui_8h.html#aaf05bd79ce0bf6796bcab2a0d2c74133a16ec7f42522b1d2ef04326ecf4543e59",
"GtaNavViewer_2imgui_2imgui_8h.html#ae62d73ae5b49cd2ef7fd4c30f37c07b6",
"RecastAssert_8h.html#a9fba2f1f45778f706f4478a73ba69e91",
"RecastMeshDetail_8cpp.html#a337ede595b48252890c745e99d559373",
"SDL__audio_8h.html#a24a02745f01041dd2825d7e26502eef2a3a26bd0e1ac5d46eda5053d0526b5fba",
"SDL__config__emscripten_8h.html#a069eb269b08ea1dfd2fbd9cdd60569a2",
"SDL__config__iphoneos_8h.html#af5e1c674bb7ab42ee349e48b33705a04",
"SDL__config__os2_8h.html#a561075a6afb23894833fc887a1552928",
"SDL__config__wingdk_8h.html#aad939a445a2185034c228c5ef388b0b4",
"SDL__events_8h.html#a3b589e89be6b35c02e0dd34a55f3fccaa1862d7d009ade2c79b2f1fe4a30c9dd2",
"SDL__haptic_8h.html#a1cd1294768bb00f35b77f67031a8a9cb",
"SDL__hints_8h.html#ac2c32b0035cb0d14111e212996d24b1a",
"SDL__keycode_8h.html#a179ce01fa41d35408f06b4b3d1cd9d3da59090b39b77aacd91a8d733153dd9475",
"SDL__messagebox_8h.html#a34d06cce5c7b8d25812da0306696de32",
"SDL__opengl_8h.html#a165997187828b3a8a0db9d49d568406b",
"SDL__opengl_8h.html#a474fd7614af2cf8fba8687fa846ed535",
"SDL__opengl_8h.html#a7a806235dc2a717e813e92583ad99b34",
"SDL__opengl_8h.html#aa9863b05e5620be66b08d31b88c54a03",
"SDL__opengl_8h.html#adde7d85b385fac85463d780f77074f8f",
"SDL__opengl__glext_8h.html#a03b9b9045d99d7b5f708123ba633fcab",
"SDL__opengl__glext_8h.html#a0f4c05beb3c5118279f68cc591d3df06",
"SDL__opengl__glext_8h.html#a19651b8f5d3ccb06579e822d4602e079",
"SDL__opengl__glext_8h.html#a242f4ab48d66c052c53adc47869b08fa",
"SDL__opengl__glext_8h.html#a2f654a610cef0663b32cd558ab578c80",
"SDL__opengl__glext_8h.html#a3a192b61218c6778fa93d6972e8f831a",
"SDL__opengl__glext_8h.html#a45c13c2871db233985caa0195a3e6b16",
"SDL__opengl__glext_8h.html#a521a993989e6d51d21bb3451414a9000",
"SDL__opengl__glext_8h.html#a5d4e1483971f0bd4d35a7e580c968f4a",
"SDL__opengl__glext_8h.html#a68df29b6719cc192d565a8576b8d79c8",
"SDL__opengl__glext_8h.html#a73b00379db2c7ac5e30a3aa2954a50ee",
"SDL__opengl__glext_8h.html#a7f1d5b2c3fd211fedb988f3f036d4503",
"SDL__opengl__glext_8h.html#a8a1628d90938a683ed20576ed195cc82",
"SDL__opengl__glext_8h.html#a9583a4a93c156492eee5ed617685a674",
"SDL__opengl__glext_8h.html#aa17330a2e675392524fb910e4718a9eb",
"SDL__opengl__glext_8h.html#aadde34fd502af6b251253c1815f35c50",
"SDL__opengl__glext_8h.html#ab808b4ff5774a18e083f000b5cef89a8",
"SDL__opengl__glext_8h.html#ac270497602f0de4a394b00b31664e29f",
"SDL__opengl__glext_8h.html#acd5dfd5ee76d3858165d704f400da5fe",
"SDL__opengl__glext_8h.html#ad8e5eee3e9e0b76a240d3f196dd188ad",
"SDL__opengl__glext_8h.html#ae341610849333ce2bd5541ad2b9c507b",
"SDL__opengl__glext_8h.html#aed74a7fa1af791e6842a75c5c2ed31ae",
"SDL__opengl__glext_8h.html#af9507cfed117a6fb4a90f1c2f8457707",
"SDL__opengles2__gl2_8h.html#a2bce1001a5d46759659381f795ec7227",
"SDL__opengles2__gl2_8h.html#aa6909d435d6628881325c52185bb877e",
"SDL__opengles2__gl2ext_8h.html#a0696cb31cfc274fc010bde30cd79a4d9",
"SDL__opengles2__gl2ext_8h.html#a242905b144383468ac50d10c7d257ec5",
"SDL__opengles2__gl2ext_8h.html#a440bcdc049e0be5cac4dff8e54868ef8",
"SDL__opengles2__gl2ext_8h.html#a65059532fb5267b0de5a8b2c04a7755c",
"SDL__opengles2__gl2ext_8h.html#a82dc6744d70243bfd1e69ddde6285d60",
"SDL__opengles2__gl2ext_8h.html#aa1219687efd2cb1d710adf536a1e1d56",
"SDL__opengles2__gl2ext_8h.html#abe2253dfe0659af59f0552fb7e723b72",
"SDL__opengles2__gl2ext_8h.html#add24c893fc041cea9ba721cb8c9f3755",
"SDL__opengles2__gl2ext_8h.html#afcd9448cf7481794825ce69ab46f2ea6",
"SDL__render_8h.html#a1b874552e4d5dcfa069256e49c6c2ae4",
"SDL__scancode_8h.html#a82ab7cff701034fb40a47b5b3a02777ba7c4526c2839f514b4e1b83c095a35a03",
"SDL__stdinc_8h.html#a42d3b826f86fc016f76c01dbdf96bce9",
"SDL__syswm_8h_source.html",
"SDL__video_8h.html#a29b118c6932ccb94d85b5435ae1e0a19",
"Sample_8h_source.html",
"classInputGeom.html#a866a12c00d1417a7403258f438a27a86",
"classSample.html#af57ce10362e08d94b16cc592734bd6ba",
"classdtFindNearestPolyQuery.html#a72c70bc181bdd635a23e0d2b6df724cb",
"dir_e11a7e58c48007658e5701ecca4dc47e.html",
"gdk.html#autotoc_md9-build-and-run-from-visual-studio",
"glad_8h.html#a237571732a7fb356792dda17d319a553",
"glad_8h.html#a488d969db88d0e5282325cd0ebe6521d",
"glad_8h.html#a6b4f3efec665cd70bb6c05223d1e5338",
"glad_8h.html#a8e03c10ccdf2060ea88469f578a9cc06",
"glad_8h.html#ab31358d0f5e2a6f39f8a2dcfdb5e0820",
"glad_8h.html#ad3e708352a87cdfef90b0c1140335be7",
"glad_8h.html#afb17f37d44cc79ab4d5ef6ff51c3a2e0",
"imgui__demo_8cpp.html#a6c822116e9dcb5956c25b1103a8a208d",
"imgui__impl__dx10_8h.html#abb1541feeb69c92b3cdf082369bb6bce",
"imgui__impl__null_8h.html#a0d294ea0ad9a51773a9f6e438876e319",
"imgui__impl__opengl3__loader_8h.html#ab6287067d6e3e85f518d59a10f19f649",
"imgui__impl__sdlgpu3_8cpp.html#afc6b77ee59f8140d077f179898edcaef",
"imgui__impl__win32_8cpp.html#ade4f55b90fdbd75a562d894d33f1909e",
"imgui__internal_8h.html#a3ead3a3e6277554127c68b60f198b50a",
"imgui__internal_8h.html#a75fe00acda5798b6ac2c09765cc5e601a250003d29e4f570aea14ddb39f8fbfa2",
"imgui__internal_8h.html#aabf7c33f50ff3dc01b14fb19578252ee",
"imgui__internal_8h.html#adc8a1d630117850fa79e11d9c8c65564",
"imgui__widgets_8cpp.html#ac8bb93c44323183c6da702c91ce950f8",
"md_Docs_2__99__Roadmap.html#short-term",
"structExampleAppLog.html#adadd24050d22189a1dc43e4a694b7ab3",
"structImDrawList.html#aa21e5a1c6e00239581f97d344fc0db61",
"structImFontConfig.html#a290a81956fdcb7ad3b5e3152594db121",
"structImGuiContext.html#a7e0dd3aef4a4f0fd85ed39e13824f2ab",
"structImGuiDockNode.html#a370dafc5228defb0f6bff36386265d62",
"structImGuiIO.html#aedd3ba8bfc9c682e5a7a66ffdd480df2",
"structImGuiMultiSelectTempData.html#ad91b0d61b263ff4a2bbabcb252930a37",
"structImGuiStorage.html#ae5ee60618c4ce8e2b4ce0e5543d52992",
"structImGuiTable.html#ad5ceb30e1aef45ce269fc661c5e89281",
"structImGuiViewportP.html#afbf514086d28ebc13634e974644cc53d",
"structImGui__ImplDX10__ViewportData.html#a2c6451986c5603d9c310141070d6039f",
"structImGui__ImplSDLGPU3__InitInfo.html#a14b17726bdf39a25140f3de29665c025",
"structImSpanAllocator.html",
"structNavMeshContext.html#a751c7b76194d33b9e21be66f1093050c",
"structSDL__ControllerButtonEvent.html#a09869d792031e47a88673d85915c209f",
"structSDL__MouseMotionEvent.html#a7674c8b92d039ab948f671a180fa7b30",
"structSampleTool.html#a305d8f95fe8f79feb757b49ff4e55ead",
"structdtTileCachePolyMesh.html#af501feeccdf8389bd7acc47e93984077",
"structstbtt__pack__range.html#a3b414cbee1e164c29dd138e0ae3d5759"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';