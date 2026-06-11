if(NOT (IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
    return()
endif()

enable_language(OBJC)

set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")

set(IOS_BUNDLE_ID "${MACOS_BUNDLE_ID}.ios" CACHE STRING "Bundle identifier for the iOS app")

set(BUILD_SERVER OFF CACHE INTERNAL "")
set(BUILD_RENDERER_GL1 ON CACHE INTERNAL "")
set(BUILD_RENDERER_GL2 OFF CACHE INTERNAL "")
set(BUILD_GAME_LIBRARIES OFF CACHE INTERNAL "")
set(USE_RENDERER_DLOPEN OFF CACHE INTERNAL "")
set(USE_OPENAL OFF CACHE INTERNAL "")
set(USE_OPENAL_DLOPEN OFF CACHE INTERNAL "")
set(USE_HTTP OFF CACHE INTERNAL "")
set(USE_VOIP OFF CACHE INTERNAL "")
set(USE_MUMBLE OFF CACHE INTERNAL "")
set(USE_FREETYPE OFF CACHE INTERNAL "")

list(APPEND CLIENT_DEFINITIONS IOS USE_GLES_FIXED)
list(APPEND RENDERER_DEFINITIONS USE_GLES_FIXED)
list(APPEND CLIENT_PLATFORM_SOURCES
    ${SOURCE_DIR}/client/cl_touch.c
    ${SOURCE_DIR}/ios/ios_layer.m
    ${SOURCE_DIR}/ios/ios_sys.m
    ${SOURCE_DIR}/ios/ios_touch_settings.m
    ${SOURCE_DIR}/ios/ios_gamepad.m
    ${SOURCE_DIR}/ios/ios_gamepad_look.c
    ${SOURCE_DIR}/ios/ios_numpad.m
    ${SOURCE_DIR}/sdl/sdl_input_ios.c
    ${SOURCE_DIR}/sdl/sdl_input_ios_gamepad.c)

list(APPEND COMMON_LIBRARIES
    "-framework Foundation"
    "-framework UIKit"
    "-framework QuartzCore"
    "-framework CoreGraphics"
    "-framework AVFoundation"
    "-framework AudioToolbox"
    "-framework CoreMotion"
    "-framework CoreHaptics"
    "-framework GameController"
    "-framework Metal")
list(APPEND RENDERER_LIBRARIES "-framework OpenGLES")

set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "" FORCE)
set(CLIENT_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
list(APPEND POST_CONFIGURE_FUNCTIONS finish_ios_app)

function(finish_ios_app)
    set_target_properties(${CLIENT_BINARY} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER ${IOS_BUNDLE_ID}
        MACOSX_BUNDLE_BUNDLE_NAME ${CLIENT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PRODUCT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PRODUCT_VERSION}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${IOS_BUNDLE_ID}
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_INFOPLIST_FILE "${CMAKE_SOURCE_DIR}/misc/ios/Info.plist"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchStoryboardName LaunchScreen
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
endfunction()
