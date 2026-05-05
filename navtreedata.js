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
"ExternC_8cpp.html#a2995bb91df6e316e4e25c5208f26f713",
"GtaNavAPI_8h.html#adb96a12177dd013ebb620aba3d990ba6",
"GtaNavViewer_2imgui_2imgui_8cpp.html#a3dc4edb949d6f68b816721d1ace9fbb5",
"GtaNavViewer_2imgui_2imgui_8h.html#a0527a5c08501cf1de0eb2c691042e40b",
"GtaNavViewer_2imgui_2imgui_8h.html#a36ab8fcad68b26863d6e910755de04c2",
"GtaNavViewer_2imgui_2imgui_8h.html#a666377a26648478a7f2bc4d3dc9cd5d1",
"GtaNavViewer_2imgui_2imgui_8h.html#aa22ffe36b188427d712447ec465203d4a0a5b0391c1edb7dd4d00802a0793d70f",
"GtaNavViewer_2imgui_2imgui_8h.html#abbc1d650f5c3ffe6af55ee82a491fa6aa2c7769fdf39c4ed20de8a74d8b33b2de",
"GtaNavViewer_2imgui_2imgui_8h.html#aef500c7434555b6cd61e14e3603c2cdaa688765339e7accd3bf36475127d133b0",
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
"classSample.html#acfb792ed987cf946d264461a04683ff7",
"classWebSockets.html#af7c26d2a259b70ce028cffb5e20799d6",
"dir_6c9d2ca142a56a0b99817d83a1904a1f.html",
"functions_~.html",
"glad_8h.html#a224db3f62a1b3e4f122071a0e9a023e2",
"glad_8h.html#a45f630b0dfee935f0aed0010167cddb5",
"glad_8h.html#a696cfaceb3256bf19e990e46702fe527",
"glad_8h.html#a8d0d00ec4423fb93b7b5176a1478e636",
"glad_8h.html#ab07df09e5eca8ad26576ebb511652ea7",
"glad_8h.html#ad2d60327bf5b20fe1d58df7915782a9f",
"glad_8h.html#af958975bb7f241884212b78aaab04e4e",
"imgui__demo_8cpp.html#a3a504542c4837c193bd5e54911123fe1",
"imgui__impl__dx10_8cpp.html#a5f74ae54c1f41e2bedec2c058074e35b",
"imgui__impl__null_8cpp.html",
"imgui__impl__opengl3__loader_8h.html#aa92826e089ca87d9e535dda14bcebc95",
"imgui__impl__sdlgpu3_8cpp.html#a4a3f1594553058de6f0079092142fb54",
"imgui__impl__win32_8cpp.html#ab4a89c5993eec0726141d49e8b105625",
"imgui__internal_8h.html#a3af79db3155dc0c16602363e83210ff8",
"imgui__internal_8h.html#a726a52b242fc9a46813045123921795fa9c8d4caf6b16c6181003dc403a4f1426",
"imgui__internal_8h.html#aa777ee3801f15acc2f81a131843039ae",
"imgui__internal_8h.html#ad9c91ba430dfd8dd924ecbeefacf6eab",
"imgui__widgets_8cpp.html#aaadd38b7c62661755d9205493a834f1e",
"md_Docs_2__99__Roadmap.html#documentation--web-presence-wip",
"structCrowdToolParams.html#a30d772dbf4f575001ff63f91bfef609c",
"structImDrawCmd.html#a8ba7df6d136db6e33d850acc9291224f",
"structImFontAtlas.html#ad5c2560d708bd0c389e9bd9da2d9b055",
"structImGuiContext.html#a3054bc5ed01af6c56159cd8de77e7b44",
"structImGuiContext.html#afcb35365c06419ca0238224ea124d6fc",
"structImGuiIO.html#a50c3ab6883647aacdf5153a4dbe2cdbe",
"structImGuiListClipperData.html",
"structImGuiPopupData.html#ac219865e04d8501f53a6b49a993b7954",
"structImGuiTable.html#a0b1d1eaeac2d85cfcdd916ca7efd85b8",
"structImGuiTextIndex.html#a17115775633756615910747494e81a8c",
"structImGuiWindowStackData.html",
"structImGui__ImplOpenGL3__Data.html#ab02168ff54e9054e1dcc662861ac20ab",
"structImGui__ImplWin32__Data.html#abe5e7a0c85bcc474f44efc6b2238fdc0",
"structImVector.html#ae2d401b4ec5f1113cdb8edb5a61a38f7",
"structSDLTest__CommonState.html#a1fcd61cbe37c2bb563e7b2f1c6853d29",
"structSDL__HapticCustom.html#a98a8995c94492069dc007502ed97eed2",
"structSDL__Surface.html#ac68afc0e3de01e3f40cb8bce2606e71b",
"structTileDbIndexEntry.html#a714a47e0e0061eeb8228cc3afaa38b8c",
"structgladGLversionStruct.html",
"structstbtt__vertex.html#aa325b3707b88e7e104c0de46bb2bf395"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';