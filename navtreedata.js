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
"ExternC_8cpp.html#a31c3b4e18ad588e205bc49cc61b2a43e",
"GtaNavContext_8h_source.html",
"GtaNavViewer_2imgui_2imgui_8cpp.html#a45a39b3a2cfa3aa899eecb63bac79d1a",
"GtaNavViewer_2imgui_2imgui_8h.html#a0a0ec0eaaaac5f97d6cd8fd7a4eb8af6a1f8c0f58375000a04813984ace6b99ea",
"GtaNavViewer_2imgui_2imgui_8h.html#a3a993a8d676ceae7ca25c4957be9600b",
"GtaNavViewer_2imgui_2imgui_8h.html#a685908b0866a60beed7ca0ced8405998",
"GtaNavViewer_2imgui_2imgui_8h.html#aa22ffe36b188427d712447ec465203d4a1d30cccf15bd66a5909f370acdfc5966",
"GtaNavViewer_2imgui_2imgui_8h.html#abbc1d650f5c3ffe6af55ee82a491fa6aada86f5c2439888502bfa5bc5caf6ade2",
"GtaNavViewer_2imgui_2imgui_8h.html#aef500c7434555b6cd61e14e3603c2cdaaeec0cd833d7db58ffa9b76f0b094b3dd",
"RecastContour_8cpp.html#ab93a3b7adad67c6fe99536760f60da7b",
"RecastMesh_8cpp.html",
"SDL__audio_8h.html#a46c5643530f8e592aeec231177aa769b",
"SDL__config__emscripten_8h.html#a1ff16bcadba18da452e2d7b2d7371e44",
"SDL__config__macosx_8h.html#a14503280ca0cb757db915eea09282bfc",
"SDL__config__os2_8h.html#a83634b0af0a402cc91fc032b38701fe0",
"SDL__config__winrt_8h.html#a0302094a0ee567c610a36c63104ebda5",
"SDL__events_8h.html#a3b589e89be6b35c02e0dd34a55f3fccaa76f91c81110f012e3a47cbbc0449e3c3",
"SDL__haptic_8h.html#a79db098a0e8cb102db6c2339a2422e29",
"SDL__hints_8h.html#ad69c295983ec1db56ea26af46f184ce7",
"SDL__keycode_8h.html#a179ce01fa41d35408f06b4b3d1cd9d3da675228e10bfb78455f9be8d6db799e24",
"SDL__messagebox_8h.html#ad21beffe204426be6efbf4990c916ad0a67ea775510f7c29ebf0a6448ed01b842",
"SDL__opengl_8h.html#a193d260b62a868cce80526c6451724a5",
"SDL__opengl_8h.html#a4aa75e08418b962e93faab0e46749923",
"SDL__opengl_8h.html#a7e2876aa7dc1dc32bc69fe70ef9e0f78",
"SDL__opengl_8h.html#aad7f4a63dea7e2cd100280ff105e90c9",
"SDL__opengl_8h.html#ae1fd330f86e60ac3438e8b1df3f7a7ba",
"SDL__opengl__glext_8h.html#a049f216c9021e2cace99b043e2e69e6a",
"SDL__opengl__glext_8h.html#a0fda3eb7f4e6d19c2fcffadc264920e7",
"SDL__opengl__glext_8h.html#a19fdccd5d1d3951cd13566c255b1f1dd",
"SDL__opengl__glext_8h.html#a253cc16b390ee2c49bcb29bdb60a4df5",
"SDL__opengl__glext_8h.html#a3010197af61d4336855ea9bad35dd953",
"SDL__opengl__glext_8h.html#a3ad5b3e82f51885b0ebc604e54b624f0",
"SDL__opengl__glext_8h.html#a46c94ba0429896780e87011658b547f3",
"SDL__opengl__glext_8h.html#a52d4a5bb40e4ab153ae8c9f992e81676",
"SDL__opengl__glext_8h.html#a5e41c6993fe09779d4c49251f6adf2eb",
"SDL__opengl__glext_8h.html#a699154088022f561f7c39b34e535069a",
"SDL__opengl__glext_8h.html#a74c46641ddd450321a197c8f7626407f",
"SDL__opengl__glext_8h.html#a802692250ed384e9fc870809f9b51fb2",
"SDL__opengl__glext_8h.html#a8afb15e935de90877c7e3eefdd28c133",
"SDL__opengl__glext_8h.html#a9676bdf3ba87e85a5476de1f2910fa6a",
"SDL__opengl__glext_8h.html#aa27099b71452de9267a21cb3311bc3d9",
"SDL__opengl__glext_8h.html#aaeb8c26a9ac4deb2f0c85ebc72f3a246",
"SDL__opengl__glext_8h.html#ab8767ba8faa141efe9905e3977a7e258",
"SDL__opengl__glext_8h.html#ac3551932d44ce11eb20c280a3f143653",
"SDL__opengl__glext_8h.html#ace6e2f67f6e8191642fb3a33c5657bfc",
"SDL__opengl__glext_8h.html#ad9e9ba4651e46b14c7f74931c77cdd16",
"SDL__opengl__glext_8h.html#ae405e157412acbcd9ceb7fac2a7cbdb0",
"SDL__opengl__glext_8h.html#aee717060b28f0bc74f16df3a87808eec",
"SDL__opengl__glext_8h.html#afa0fb1b5e976920c0abeff2dca3ed774",
"SDL__opengles2__gl2_8h.html#a318a0c53e89e0bc5fdd162a405f77a9d",
"SDL__opengles2__gl2_8h.html#aad590dd79075659a5625dcad04e13331",
"SDL__opengles2__gl2ext_8h.html#a08b56773e45dbaa9f0aaf79ed375f8ce",
"SDL__opengles2__gl2ext_8h.html#a25f44ebb6d1fff7c5c4e8a47f5435d68",
"SDL__opengles2__gl2ext_8h.html#a458c3dbebcab23061588f8c6340ed249",
"SDL__opengles2__gl2ext_8h.html#a66e2821fb869fb1ec952e1ffb7531067",
"SDL__opengles2__gl2ext_8h.html#a85e827a457473092083499553cdab504",
"SDL__opengles2__gl2ext_8h.html#aa2c3ef4a4badbecb713410fd5ab0451e",
"SDL__opengles2__gl2ext_8h.html#ac063028cac5ec8c90c76b4db9bdff0f7",
"SDL__opengles2__gl2ext_8h.html#adf2e7ed56b97b128479c2bdd6a90880b",
"SDL__opengles2__gl2ext_8h.html#afe90587f6b71751de2e268b1edce2590",
"SDL__render_8h.html#a435c61d9f048550388198478e8fdeaa0",
"SDL__scancode_8h.html#a82ab7cff701034fb40a47b5b3a02777ba8be476e6c46701dc84db07dc412d22f2",
"SDL__stdinc_8h.html#a5a269b805581a775264f7b179d9c33a8",
"SDL__test__common_8h.html#a2ec5c6d6329602f002d441de8edfdad2",
"SDL__video_8h.html#a2de24951bbc6626dc259ec0db5ae8ed4a901dce8f81e72cd6c0efa1f32ac96c2d",
"Sample__TempObstacles_8cpp.html#a877a1485eecb109f3d6431356bf30e94",
"classInputGeom.html#acc866de7fce6ca643c6695cb1f1d3967",
"classSample__Debug.html#a230fb5fe100409fc376604e0a470e259",
"classdtLocalBoundary.html",
"dir_f989b3d3473f3227bb3c0f171476a0db.html",
"glad_8h.html",
"glad_8h.html#a241ccb34c87933f5df602ab8129319cb",
"glad_8h.html#a49ab85bf98c6977061382dae1689bf43",
"glad_8h.html#a6d53cfb9b7cbe9abb9253afcea445b04",
"glad_8h.html#a8f71970fd333935c9b9bf2440d5eaf7d",
"glad_8h.html#ab3e702cb6674aec6967dd262580942cb",
"glad_8h.html#ad4de17abefa73fa2ef53379f73b734be",
"glad_8h.html#afbddad8bdc3f924d11cbf6e0dce15177",
"imgui__demo_8cpp.html#a7ea459383a527c89e210e578e85ef5c8",
"imgui__impl__dx11_8cpp.html",
"imgui__impl__null_8h.html#a6b8003dc450f70aba9eabc4ba4be6232",
"imgui__impl__opengl3__loader_8h.html#abca6d43f43fae31f49dcb883b2f301f6",
"imgui__impl__sdlgpu3_8h.html#a72261bdf3e62e42f5fd71dc781f5aa7c",
"imgui__impl__win32_8cpp.html#af46d1fa2c9d2dc52fa9d62421f6c3893",
"imgui__internal_8h.html#a3fab98556d11690efd4f7f2fde94968e",
"imgui__internal_8h.html#a75fe00acda5798b6ac2c09765cc5e601a462b0ceb6e0a3c234a10b8de054dc373",
"imgui__internal_8h.html#aacc86f220858e37a583fee458c4d709ba86ca1f5ab44fc267491e0cf7518dd883",
"imgui__internal_8h.html#ade001ee6028075c9c1e2bd3f0395b920",
"imgui__widgets_8cpp.html#acffcd64b4f14cc09b7b1339ce67a7563",
"md_external_2SDL2_2docs_2CONTRIBUTING.html",
"structDirtyEntry.html#a6efa59792bbe3b1feea05da8110151dd",
"structImDrawData.html#afe3a34d1f384c37e99627e6efbc91815",
"structImFontAtlasBuilder.html#ac2f643d8abc462e7abec1126706663db",
"structImGuiContext.html#a4a370ec661b7ed0372fadf8a8247519d",
"structImGuiDebugAllocInfo.html#a08ac2748c718e2d93de7abe16928e2a3",
"structImGuiIO.html#a80dcb0e301e2df7c976737faec0bea1d",
"structImGuiMenuColumns.html#aae99240b9c8e9380f829921da5e337f2",
"structImGuiSelectionBasicStorage.html#ab55dc39b0ce40405957af6ff0086f4af",
"structImGuiTable.html#a478781580df4716cfe31e8d639c6f2c5",
"structImGuiTypingSelectState.html#a7fde026be5ff298793d0dd7bceff2bfe",
"structImGuiWindowTempData.html#a684f9cd8726dc846a633011d22367e90",
"structImGui__ImplSDL2__Data.html#ac8fc9cb6a1e473e872a2deb9c19df8a8",
"structImPool.html#a7b2c095ea540dc8afb670e705a37a912",
"structLinearAllocator.html#acfe183e62f5da36d3ce0fd75e59ecb92",
"structSDLTest__CommonState.html#a8a90266f8d6dc7846fbd3368426caec1",
"structSDL__HapticPeriodic.html#aad0ad2b00c7e6959da627663d91f7b94",
"structSDL__TextEditingExtEvent.html#a404d784703abd48fb71e1099bde114a3",
"structTileGridStats.html#aa37752645b99bd2436310b171aa43e4a",
"structimguiGfxText.html#ad7bf3cc0e24e63d997c335b7a5e15d25",
"unionImGL3WProcs.html#a4a4589063c11e88d151c3e63fe208c9f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';