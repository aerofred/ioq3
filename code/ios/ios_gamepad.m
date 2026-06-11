#include "ios_gamepad.h"
#include "../client/client.h"
#include "../client/cl_touch.h"
#include "../sdl/sdl_input_ios_gamepad.h"
#include "../sys/sys_local.h"
#include "ios_gamepad_look.h"

#import <UIKit/UIKit.h>
#import <GameController/GameController.h>

static void IOS_Gamepad_RefreshNativePresent( void );

static NSString * const kGamepadBindingsKey = @"ioq3GamepadBindings";
static NSString * const kGamepadSensitivityKey = @"ioq3GamepadSensitivity";
static NSString * const kGamepadDeadZoneKey = @"ioq3GamepadDeadZone";
static NSString * const kGamepadLookAccelKey = @"ioq3GamepadLookAccel";
static NSString * const kGamepadLookInvertKey = @"ioq3GamepadLookInvert";

static NSDictionary<NSString *, NSString *> *IOS_GamepadDefaultBindings( void )
{
	return @{
		@"PAD0_RIGHTTRIGGER": @"+attack",
		@"PAD0_LEFTTRIGGER": @"+zoom",
		@"PAD0_LEFTSTICK_UP": @"+forward",
		@"PAD0_LEFTSTICK_DOWN": @"+back",
		@"PAD0_LEFTSTICK_LEFT": @"+moveleft",
		@"PAD0_LEFTSTICK_RIGHT": @"+moveright",
		@"PAD0_A": @"+moveup",
		@"PAD0_B": @"+movedown",
		@"PAD0_X": @"+use",
		@"PAD0_Y": @"weapprev",
		@"PAD0_LEFTSHOULDER": @"weapprev",
		@"PAD0_RIGHTSHOULDER": @"weapnext",
		@"PAD0_DPAD_UP": @"weapprev",
		@"PAD0_DPAD_DOWN": @"weapnext",
		@"PAD0_START": @"togglemenu"
	};
}

static NSDictionary<NSString *, NSString *> *IOS_GamepadInputDisplayNames( void )
{
	return @{
		@"PAD0_A": @"A",
		@"PAD0_B": @"B",
		@"PAD0_X": @"X",
		@"PAD0_Y": @"Y",
		@"PAD0_BACK": @"Back",
		@"PAD0_GUIDE": @"Guide",
		@"PAD0_START": @"Start",
		@"PAD0_LEFTSTICK_CLICK": @"L3",
		@"PAD0_RIGHTSTICK_CLICK": @"R3",
		@"PAD0_LEFTSHOULDER": @"LB",
		@"PAD0_RIGHTSHOULDER": @"RB",
		@"PAD0_DPAD_UP": @"D-Pad Up",
		@"PAD0_DPAD_DOWN": @"D-Pad Down",
		@"PAD0_DPAD_LEFT": @"D-Pad Left",
		@"PAD0_DPAD_RIGHT": @"D-Pad Right",
		@"PAD0_LEFTSTICK_UP": @"Left Stick Up",
		@"PAD0_LEFTSTICK_DOWN": @"Left Stick Down",
		@"PAD0_LEFTSTICK_LEFT": @"Left Stick Left",
		@"PAD0_LEFTSTICK_RIGHT": @"Left Stick Right",
		@"PAD0_RIGHTSTICK_UP": @"Right Stick Up",
		@"PAD0_RIGHTSTICK_DOWN": @"Right Stick Down",
		@"PAD0_RIGHTSTICK_LEFT": @"Right Stick Left",
		@"PAD0_RIGHTSTICK_RIGHT": @"Right Stick Right",
		@"PAD0_LEFTTRIGGER": @"LT",
		@"PAD0_RIGHTTRIGGER": @"RT"
	};
}

@interface SGGamepadConfig : NSObject
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSString *> *bindings;
@property (nonatomic) float sensitivity;
@property (nonatomic) float deadZone;
@property (nonatomic) float lookAcceleration;
@property (nonatomic) BOOL lookInverted;
+ (instancetype)shared;
- (NSString *)displayNameForInput:(NSString *)input;
- (NSString *)inputForCommand:(NSString *)command;
- (void)setBindingInput:(NSString *)input command:(NSString *)command;
- (void)clearBindingForCommand:(NSString *)command;
- (void)resetToDefaults;
- (void)persist;
- (void)appendLaunchCommandsTo:(NSMutableString *)buffer;
- (void)applyToRunningEngine;
@end

@implementation SGGamepadConfig

+ (instancetype)shared {
	static SGGamepadConfig *instance;
	static dispatch_once_t once;
	dispatch_once(&once, ^{ instance = [[SGGamepadConfig alloc] init]; });
	return instance;
}

- (instancetype)init {
	self = [super init];
	if ( !self ) return nil;

	NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
	NSDictionary *saved = [defaults dictionaryForKey:kGamepadBindingsKey];
	NSMutableDictionary *merged = [IOS_GamepadDefaultBindings() mutableCopy];

	if ( saved ) {
		for ( NSString *key in saved ) {
			NSString *value = saved[key];
			if ( value.length == 0 ) {
				[merged removeObjectForKey:key];
			} else {
				merged[key] = value;
			}
		}
	}

	_bindings = merged;

	if ( [defaults objectForKey:kGamepadSensitivityKey] == nil ) {
		_sensitivity = 10.0f;
	} else {
		_sensitivity = (float)[defaults doubleForKey:kGamepadSensitivityKey];
	}

	if ( [defaults objectForKey:kGamepadDeadZoneKey] == nil ) {
		_deadZone = 0.15f;
	} else {
		_deadZone = (float)[defaults doubleForKey:kGamepadDeadZoneKey];
	}

	if ( [defaults objectForKey:kGamepadLookAccelKey] == nil ) {
		_lookAcceleration = 2.0f;
	} else {
		_lookAcceleration = (float)[defaults doubleForKey:kGamepadLookAccelKey];
	}

	if ( [defaults objectForKey:kGamepadLookInvertKey] == nil ) {
		_lookInverted = NO;
	} else {
		_lookInverted = [defaults boolForKey:kGamepadLookInvertKey];
	}

	return self;
}

- (NSString *)displayNameForInput:(NSString *)input {
	if ( input.length == 0 ) return @"Unbound";
	return IOS_GamepadInputDisplayNames()[input] ?: input;
}

- (NSString *)inputForCommand:(NSString *)command {
	for ( NSString *key in self.bindings ) {
		if ( [self.bindings[key] isEqualToString:command] ) {
			return key;
		}
	}
	return nil;
}

- (void)setBindingInput:(NSString *)input command:(NSString *)command {
	for ( NSString *key in [self.bindings copy] ) {
		if ( [self.bindings[key] isEqualToString:command] ) {
			self.bindings[key] = @"";
		}
	}
	self.bindings[input] = command;
	[self persist];
}

- (void)clearBindingForCommand:(NSString *)command {
	for ( NSString *key in [self.bindings copy] ) {
		if ( [self.bindings[key] isEqualToString:command] ) {
			self.bindings[key] = @"";
		}
	}
	[self persist];
}

- (void)resetToDefaults {
	self.bindings = [IOS_GamepadDefaultBindings() mutableCopy];
	self.sensitivity = 10.0f;
	self.deadZone = 0.15f;
	self.lookAcceleration = 2.0f;
	self.lookInverted = NO;
	[self persist];
}

- (void)persist {
	NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
	[defaults setObject:self.bindings forKey:kGamepadBindingsKey];
	[defaults setDouble:self.sensitivity forKey:kGamepadSensitivityKey];
	[defaults setDouble:self.deadZone forKey:kGamepadDeadZoneKey];
	[defaults setDouble:self.lookAcceleration forKey:kGamepadLookAccelKey];
	[defaults setBool:self.lookInverted forKey:kGamepadLookInvertKey];
}

- (void)appendLaunchCommandsTo:(NSMutableString *)buffer {
	[buffer appendFormat:@"+set in_joystick 1 +set in_joystickUseAnalog 0 "
		@"+set sensitivity %.1f +set joy_threshold %.2f "
		@"+set in_gamepadLookAccel %.1f +set in_gamepadLookScale 12.0 "
		@"+set in_gamepadLookInvert %d ",
		self.sensitivity, self.deadZone, self.lookAcceleration, self.lookInverted ? 1 : 0];

	NSArray *keys = [[self.bindings allKeys] sortedArrayUsingSelector:@selector(compare:)];
	for ( NSString *input in keys ) {
		NSString *command = self.bindings[input];
		if ( command.length == 0 ) continue;
		/* Quote bind values: Com_ParseCommandLine splits on '+' outside quotes. */
		[buffer appendFormat:@"+bind %@ \"%@\" ", input, command];
	}

	[buffer appendString:@"+set j_yaw_axis 2 +set j_pitch_axis 3 +set j_yaw 0 +set j_pitch 0 "
		@"+set j_side_axis 4 +set j_side 0.25 +set j_forward_axis 1 +set j_forward -2 +set cl_run 1 "];
}

- (void)appendEngineCommandsTo:(NSMutableString *)buffer {
	[buffer appendFormat:@"seta in_joystick 1; seta in_joystickUseAnalog 0; "
		@"seta sensitivity %.1f; seta joy_threshold %.2f; "
		@"seta in_gamepadLookAccel %.1f; seta in_gamepadLookScale 12.0; "
		@"seta in_gamepadLookInvert %d; ",
		self.sensitivity, self.deadZone, self.lookAcceleration, self.lookInverted ? 1 : 0];

	NSArray *keys = [[self.bindings allKeys] sortedArrayUsingSelector:@selector(compare:)];
	for ( NSString *input in keys ) {
		NSString *command = self.bindings[input];
		if ( command.length == 0 ) continue;
		[buffer appendFormat:@"bind %@ \"%@\"; ", input, command];
	}

	[buffer appendString:@"seta j_yaw_axis 2; seta j_pitch_axis 3; seta j_yaw 0; seta j_pitch 0; "
		@"seta j_side_axis 4; seta j_side 0.25; seta j_forward_axis 1; seta j_forward -2; seta cl_run 1; "];
}

- (void)applyToRunningEngine {
	NSMutableString *commands = [NSMutableString string];
	[self appendEngineCommandsTo:commands];
	[commands appendString:@"\n"];
	CL_ExecuteConsole( commands.UTF8String );
	IOS_Gamepad_ReloadKeyNumbers();
}

@end

@interface SGGamepadInputManager : NSObject
@property (nonatomic) BOOL isRunning;
@property (nonatomic) BOOL isConfigCaptureActive;
@property (nonatomic) BOOL onScreenMoveJoystickEngaged;
@property (nonatomic, weak) GCController *attachedController;
@end

@implementation SGGamepadInputManager {
	NSMutableDictionary<NSString *, NSNumber *> *_keyNums;
	NSMutableSet<NSNumber *> *_pressedKeys;
	NSMutableSet<NSNumber *> *_stickKeysDown;
	BOOL _isManagingMoveAxes;
	BOOL _usingSDLEnginePath;
	NSTimer *_pollTimer;
	NSTimer *_discoveryTimer;
	id _connectObserver;
	id _disconnectObserver;
	id _activeObserver;
}

+ (instancetype)shared {
	static SGGamepadInputManager *instance;
	static dispatch_once_t once;
	dispatch_once(&once, ^{ instance = [[SGGamepadInputManager alloc] init]; });
	return instance;
}

- (instancetype)init {
	self = [super init];
	if ( self ) {
		_keyNums = [NSMutableDictionary dictionary];
		_pressedKeys = [NSMutableSet set];
		_stickKeysDown = [NSMutableSet set];
		[self reloadKeyNumbers];
	}
	return self;
}

- (void)reloadKeyNumbers {
	[_keyNums removeAllObjects];
	for ( NSString *name in [IOS_GamepadInputDisplayNames() allKeys] ) {
		int keynum = Key_StringToKeynum( (char *)name.UTF8String );
		if ( keynum >= 0 ) {
			_keyNums[name] = @(keynum);
		}
	}
}

- (float)deadZone {
	float configured = SGGamepadConfig.shared.deadZone;
	float cvarValue = CL_GetCvarFloat( "joy_threshold" );
	if ( cvarValue > 0.01f ) {
		return MAX( configured, cvarValue );
	}
	return configured;
}

- (BOOL)shouldProcessInput {
	if ( self.isConfigCaptureActive ) return NO;
	if ( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_MESSAGE ) ) return NO;
	return YES;
}

- (void)sendMoveAxesYaw:(int)yaw forward:(int)forward {
	unsigned time = Sys_Milliseconds();
	CL_JoystickEvent( 0, yaw, time );
	CL_JoystickEvent( 1, forward, time );
}

- (void)setKey:(int)keynum down:(BOOL)down {
	NSNumber *num = @(keynum);
	BOOL wasDown = [_pressedKeys containsObject:num];
	if ( down == wasDown ) return;

	if ( down ) {
		[_pressedKeys addObject:num];
	} else {
		[_pressedKeys removeObject:num];
	}
	CL_KeyEvent( keynum, down ? qtrue : qfalse, Sys_Milliseconds() );
}

- (void)setPadKey:(NSString *)name down:(BOOL)down {
	NSNumber *keynum = _keyNums[name];
	if ( !keynum ) return;
	[self setKey:keynum.intValue down:down];
}

- (void)updateStickKey:(NSString *)name down:(BOOL)down {
	NSNumber *keynum = _keyNums[name];
	if ( !keynum ) return;
	NSNumber *num = keynum;
	BOOL wasDown = [_stickKeysDown containsObject:num];
	if ( down == wasDown ) return;
	if ( down ) {
		[_stickKeysDown addObject:num];
	} else {
		[_stickKeysDown removeObject:num];
	}
	[self setKey:keynum.intValue down:down];
}

- (void)releaseManagedKeys {
	for ( NSNumber *keynum in [_stickKeysDown copy] ) {
		CL_KeyEvent( keynum.intValue, qfalse, Sys_Milliseconds() );
	}
	[_stickKeysDown removeAllObjects];
	for ( NSNumber *keynum in [_pressedKeys copy] ) {
		CL_KeyEvent( keynum.intValue, qfalse, Sys_Milliseconds() );
	}
	[_pressedKeys removeAllObjects];
}

- (void)releaseMoveAxesIfManaging {
	if ( !_isManagingMoveAxes ) return;
	if ( !self.onScreenMoveJoystickEngaged ) {
		[self sendMoveAxesYaw:0 forward:0];
	}
	_isManagingMoveAxes = NO;
}

- (float)applyDeadzone:(float)value threshold:(float)threshold {
	if ( fabsf( value ) < threshold ) return 0.0f;
	float sign = value < 0.0f ? -1.0f : 1.0f;
	float scaled = ( fabsf( value ) - threshold ) / MAX( 0.01f, 1.0f - threshold );
	return sign * MIN( 1.0f, scaled );
}

- (void)handleGamepad:(GCExtendedGamepad *)pad {
	if ( ![self shouldProcessInput] ) {
		[self releaseManagedKeys];
		[self releaseMoveAxesIfManaging];
		return;
	}

	[self setPadKey:@"PAD0_A" down:pad.buttonA.isPressed];
	[self setPadKey:@"PAD0_B" down:pad.buttonB.isPressed];
	[self setPadKey:@"PAD0_X" down:pad.buttonX.isPressed];
	[self setPadKey:@"PAD0_Y" down:pad.buttonY.isPressed];
	[self setPadKey:@"PAD0_LEFTSHOULDER" down:pad.leftShoulder.isPressed];
	[self setPadKey:@"PAD0_RIGHTSHOULDER" down:pad.rightShoulder.isPressed];
	[self setPadKey:@"PAD0_DPAD_UP" down:pad.dpad.up.isPressed];
	[self setPadKey:@"PAD0_DPAD_DOWN" down:pad.dpad.down.isPressed];
	[self setPadKey:@"PAD0_DPAD_LEFT" down:pad.dpad.left.isPressed];
	[self setPadKey:@"PAD0_DPAD_RIGHT" down:pad.dpad.right.isPressed];

	if ( @available(iOS 12.1, *) ) {
		[self setPadKey:@"PAD0_LEFTSTICK_CLICK" down:pad.leftThumbstickButton.isPressed];
		[self setPadKey:@"PAD0_RIGHTSTICK_CLICK" down:pad.rightThumbstickButton.isPressed];
	}
	if ( @available(iOS 13.0, *) ) {
		[self setPadKey:@"PAD0_START" down:pad.buttonMenu.isPressed];
		[self setPadKey:@"PAD0_BACK" down:pad.buttonOptions.isPressed];
	}

	float threshold = [self deadZone];
	[self setPadKey:@"PAD0_LEFTTRIGGER" down:( pad.leftTrigger.value > threshold )];
	[self setPadKey:@"PAD0_RIGHTTRIGGER" down:( pad.rightTrigger.value > threshold )];

	BOOL dpadActive = pad.dpad.up.isPressed || pad.dpad.down.isPressed ||
		pad.dpad.left.isPressed || pad.dpad.right.isPressed;

	if ( dpadActive ) {
		[self updateStickKey:@"PAD0_LEFTSTICK_LEFT" down:NO];
		[self updateStickKey:@"PAD0_LEFTSTICK_RIGHT" down:NO];
		[self updateStickKey:@"PAD0_LEFTSTICK_UP" down:NO];
		[self updateStickKey:@"PAD0_LEFTSTICK_DOWN" down:NO];
		[self releaseMoveAxesIfManaging];
	} else {
		float lx = [self applyDeadzone:pad.leftThumbstick.xAxis.value threshold:threshold];
		float ly = [self applyDeadzone:pad.leftThumbstick.yAxis.value threshold:threshold];
		[self updateStickKey:@"PAD0_LEFTSTICK_LEFT" down:( pad.leftThumbstick.xAxis.value < -threshold )];
		[self updateStickKey:@"PAD0_LEFTSTICK_RIGHT" down:( pad.leftThumbstick.xAxis.value > threshold )];

		if ( fabsf( lx ) > 0.01f || fabsf( ly ) > 0.01f ) {
			float sensitivity = MAX( 0.25f, CL_GetCvarFloat( "in_touchMoveSensitivity" ) );
			int forward = (int)lrintf( ly * 127.0f * sensitivity );
			forward = MAX( -127, MIN( 127, forward ) );
			[self sendMoveAxesYaw:0 forward:forward];
			_isManagingMoveAxes = YES;
		} else {
			[self updateStickKey:@"PAD0_LEFTSTICK_UP" down:NO];
			[self updateStickKey:@"PAD0_LEFTSTICK_DOWN" down:NO];
			[self releaseMoveAxesIfManaging];
		}
	}

	[self updateStickKey:@"PAD0_RIGHTSTICK_UP" down:NO];
	[self updateStickKey:@"PAD0_RIGHTSTICK_DOWN" down:NO];
	[self updateStickKey:@"PAD0_RIGHTSTICK_LEFT" down:NO];
	[self updateStickKey:@"PAD0_RIGHTSTICK_RIGHT" down:NO];
	IOS_Gamepad_LookFromStick( pad.rightThumbstick.xAxis.value, pad.rightThumbstick.yAxis.value );
}

- (BOOL)tryActivateSDLEnginePath {
	IN_IosRefreshJoystick( qfalse );
	if ( Sys_SDLJoystickCount() <= 0 ) return NO;
	if ( _usingSDLEnginePath && Sys_SDLGamepadOpened() > 0 ) return YES;
	if ( !self.isRunning || !com_fullyInitialized ) return NO;

	[self detachSwiftHandler];
	Sys_SetNativeGamepadActive( qfalse );
	IN_IosRefreshJoystick( qtrue );
	if ( Sys_SDLGamepadOpened() <= 0 ) return NO;

		_usingSDLEnginePath = YES;
		[SGGamepadConfig.shared applyToRunningEngine];
		[_discoveryTimer invalidate];
		_discoveryTimer = nil;
		IOS_Gamepad_SetNativePresent( qtrue );
		return YES;
}

- (void)detachSwiftHandler {
	[_pollTimer invalidate];
	_pollTimer = nil;
	self.attachedController.extendedGamepad.valueChangedHandler = nil;
	self.attachedController = nil;
	Sys_SetNativeGamepadActive( qfalse );
	[self releaseManagedKeys];
	[self releaseMoveAxesIfManaging];
}

- (void)startPollTimerForGamepad:(GCExtendedGamepad *)gamepad {
	[_pollTimer invalidate];
	__weak typeof(self) weakSelf = self;
	_pollTimer = [NSTimer scheduledTimerWithTimeInterval:( 1.0 / 60.0 ) repeats:YES block:^( NSTimer *timer ) {
		[weakSelf handleGamepad:gamepad];
	}];
}

- (BOOL)attachHandlerToController:(GCController *)controller {
	if ( self.attachedController ) return NO;
	GCExtendedGamepad *gamepad = controller.extendedGamepad;
	if ( !gamepad ) return NO;
	if ( [self tryActivateSDLEnginePath] ) return YES;

	self.attachedController = controller;
	__weak typeof(self) weakSelf = self;
	gamepad.valueChangedHandler = ^( GCExtendedGamepad *pad, GCControllerElement *element ) {
		[weakSelf handleGamepad:pad];
	};
	[self startPollTimerForGamepad:gamepad];
	Sys_SetNativeGamepadActive( qtrue );
	[SGGamepadConfig.shared applyToRunningEngine];
	[self handleGamepad:gamepad];
	IOS_Gamepad_SetNativePresent( qtrue );
	return YES;
}

- (void)attachHandlers {
	if ( _usingSDLEnginePath ) return;
	if ( [self tryActivateSDLEnginePath] ) return;
	for ( GCController *controller in GCController.controllers ) {
		if ( [self attachHandlerToController:controller] ) return;
	}
}

- (void)installObserversIfNeeded {
	if ( _connectObserver ) return;

	__weak typeof(self) weakSelf = self;
	_connectObserver = [NSNotificationCenter.defaultCenter addObserverForName:GCControllerDidConnectNotification
		object:nil queue:NSOperationQueue.mainQueue usingBlock:^( NSNotification *note ) {
			GCController *controller = note.object;
			if ( weakSelf.isRunning ) {
				IOS_Gamepad_SetNativePresent( qtrue );
				if ( ![weakSelf tryActivateSDLEnginePath] ) {
					[weakSelf attachHandlerToController:controller];
				}
			}
		}];

		_disconnectObserver = [NSNotificationCenter.defaultCenter addObserverForName:GCControllerDidDisconnectNotification
		object:nil queue:NSOperationQueue.mainQueue usingBlock:^( NSNotification *note ) {
			if ( weakSelf.attachedController == note.object ) {
				[weakSelf detachSwiftHandler];
				_usingSDLEnginePath = NO;
				Sys_SetNativeGamepadActive( qfalse );
				IOS_Gamepad_NotifyDeviceChange();
			}
		}];

	_activeObserver = [NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidBecomeActiveNotification
		object:nil queue:NSOperationQueue.mainQueue usingBlock:^( NSNotification *note ) {
			if ( weakSelf.isRunning ) {
				[GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
				if ( ![weakSelf tryActivateSDLEnginePath] ) {
					[weakSelf attachHandlers];
				}
			}
		}];
}

- (void)start {
	self.isRunning = YES;
	[self installObserversIfNeeded];
	[GCController startWirelessControllerDiscoveryWithCompletionHandler:^( void ) {
		if ( ![self tryActivateSDLEnginePath] ) {
			[self attachHandlers];
		}
		IOS_Gamepad_RefreshNativePresent();
	}];
	[self reloadKeyNumbers];
	if ( ![self tryActivateSDLEnginePath] ) {
		[self attachHandlers];
	}
	IOS_Gamepad_RefreshNativePresent();
}

- (void)pauseForOverlay {
	[self releaseManagedKeys];
	[self releaseMoveAxesIfManaging];
}

- (void)stop {
	self.isRunning = NO;
	[_discoveryTimer invalidate];
	_discoveryTimer = nil;
	[self detachSwiftHandler];
	_usingSDLEnginePath = NO;
	Sys_SetNativeGamepadActive( qfalse );
	IOS_Gamepad_SetNativePresent( qfalse );
}

@end

void IOS_Gamepad_ReloadKeyNumbers( void )
{
	[SGGamepadInputManager.shared reloadKeyNumbers];
}

static volatile qboolean ios_gamepadNativePresent = qfalse;

static void IOS_Gamepad_OnMain( dispatch_block_t block )
{
	if ( [NSThread isMainThread] ) {
		block();
	} else {
		dispatch_async( dispatch_get_main_queue(), block );
	}
}

void IOS_Gamepad_SetNativePresent( qboolean present )
{
	ios_gamepadNativePresent = present;
}

qboolean IOS_Gamepad_IsActive( void )
{
	if ( Sys_SDLGamepadOpened() > 0 ) {
		return qtrue;
	}
	return ios_gamepadNativePresent;
}

static void IOS_Gamepad_RefreshNativePresent( void )
{
	IOS_Gamepad_OnMain( ^{
		qboolean present = GCController.controllers.count > 0;
		if ( Sys_SDLGamepadOpened() > 0 ) {
			present = qtrue;
		} else if ( [SGGamepadInputManager.shared attachedController] ) {
			present = qtrue;
		}
		IOS_Gamepad_SetNativePresent( present ? qtrue : qfalse );
	} );
}

void IOS_Gamepad_NotifyDeviceChange( void )
{
	IOS_Gamepad_RefreshNativePresent();
}

void IOS_Gamepad_PrepareAtLaunch( void )
{
	IOS_Gamepad_OnMain( ^{
		if ( @available(iOS 14.5, *) ) {
			GCController.shouldMonitorBackgroundEvents = YES;
		}
		[GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
	} );
}

void IOS_Gamepad_Start( void )
{
	IOS_Gamepad_OnMain( ^{
		[SGGamepadInputManager.shared start];
	} );
}

void IOS_Gamepad_Stop( void )
{
	IOS_Gamepad_OnMain( ^{
		[SGGamepadInputManager.shared stop];
	} );
}

void IOS_Gamepad_PauseForOverlay( void )
{
	IOS_Gamepad_OnMain( ^{
		[SGGamepadInputManager.shared pauseForOverlay];
	} );
}

void IOS_Gamepad_SetOnScreenMoveEngaged( qboolean engaged )
{
	SGGamepadInputManager.shared.onScreenMoveJoystickEngaged = engaged ? YES : NO;
}

qboolean IOS_Gamepad_IsConfigCaptureActive( void )
{
	return SGGamepadInputManager.shared.isConfigCaptureActive ? qtrue : qfalse;
}

void IOS_Gamepad_SetConfigCaptureActive( qboolean active )
{
	SGGamepadInputManager.shared.isConfigCaptureActive = active ? YES : NO;
}

void IOS_Gamepad_ApplyLaunchConfig( char *commandLine, int commandLineSize )
{
	NSMutableString *buffer = [NSMutableString string];
	[[SGGamepadConfig shared] appendLaunchCommandsTo:buffer];
	Q_strcat( commandLine, commandLineSize, buffer.UTF8String );
}

@interface SGGamepadCapture : NSObject
@property (nonatomic, copy) void (^onInputCaptured)(NSString *input);
- (void)start;
- (void)stop;
@end

@implementation SGGamepadCapture {
	GCController *_attachedController;
	id _connectObserver;
}

- (void)start {
	IOS_Gamepad_SetConfigCaptureActive( qtrue );
	[self stopHandlers];
	[GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
	[self attachToController:GCController.controllers.firstObject];

	__weak typeof(self) weakSelf = self;
	_connectObserver = [NSNotificationCenter.defaultCenter addObserverForName:GCControllerDidConnectNotification
		object:nil queue:NSOperationQueue.mainQueue usingBlock:^( NSNotification *note ) {
			[weakSelf attachToController:note.object];
		}];
}

- (void)stop {
	IOS_Gamepad_SetConfigCaptureActive( qfalse );
	if ( _connectObserver ) {
		[NSNotificationCenter.defaultCenter removeObserver:_connectObserver];
		_connectObserver = nil;
	}
	[self stopHandlers];
}

- (void)attachToController:(GCController *)controller {
	[self stopHandlers];
	GCExtendedGamepad *gamepad = controller.extendedGamepad;
	if ( !gamepad ) return;
	_attachedController = controller;
	__weak typeof(self) weakSelf = self;
	gamepad.valueChangedHandler = ^( GCExtendedGamepad *pad, GCControllerElement *element ) {
		[weakSelf processGamepad:pad];
	};
}

- (void)stopHandlers {
	_attachedController.extendedGamepad.valueChangedHandler = nil;
	_attachedController = nil;
}

- (void)capture:(NSString *)input {
	if ( self.onInputCaptured ) self.onInputCaptured( input );
}

- (void)processGamepad:(GCExtendedGamepad *)pad {
	float threshold = 0.55f;
	if ( pad.buttonA.isPressed ) { [self capture:@"PAD0_A"]; return; }
	if ( pad.buttonB.isPressed ) { [self capture:@"PAD0_B"]; return; }
	if ( pad.buttonX.isPressed ) { [self capture:@"PAD0_X"]; return; }
	if ( pad.buttonY.isPressed ) { [self capture:@"PAD0_Y"]; return; }
	if ( pad.leftShoulder.isPressed ) { [self capture:@"PAD0_LEFTSHOULDER"]; return; }
	if ( pad.rightShoulder.isPressed ) { [self capture:@"PAD0_RIGHTSHOULDER"]; return; }
	if ( @available(iOS 12.1, *) ) {
		if ( pad.leftThumbstickButton.isPressed ) { [self capture:@"PAD0_LEFTSTICK_CLICK"]; return; }
		if ( pad.rightThumbstickButton.isPressed ) { [self capture:@"PAD0_RIGHTSTICK_CLICK"]; return; }
	}
	if ( pad.dpad.up.isPressed ) { [self capture:@"PAD0_DPAD_UP"]; return; }
	if ( pad.dpad.down.isPressed ) { [self capture:@"PAD0_DPAD_DOWN"]; return; }
	if ( pad.dpad.left.isPressed ) { [self capture:@"PAD0_DPAD_LEFT"]; return; }
	if ( pad.dpad.right.isPressed ) { [self capture:@"PAD0_DPAD_RIGHT"]; return; }
	if ( pad.leftTrigger.value > threshold ) { [self capture:@"PAD0_LEFTTRIGGER"]; return; }
	if ( pad.rightTrigger.value > threshold ) { [self capture:@"PAD0_RIGHTTRIGGER"]; return; }
	float lx = pad.leftThumbstick.xAxis.value;
	float ly = pad.leftThumbstick.yAxis.value;
	if ( ly > threshold ) { [self capture:@"PAD0_LEFTSTICK_UP"]; return; }
	if ( ly < -threshold ) { [self capture:@"PAD0_LEFTSTICK_DOWN"]; return; }
	if ( lx < -threshold ) { [self capture:@"PAD0_LEFTSTICK_LEFT"]; return; }
	if ( lx > threshold ) { [self capture:@"PAD0_LEFTSTICK_RIGHT"]; return; }
	float rx = pad.rightThumbstick.xAxis.value;
	float ry = pad.rightThumbstick.yAxis.value;
	if ( ry > threshold ) { [self capture:@"PAD0_RIGHTSTICK_UP"]; return; }
	if ( ry < -threshold ) { [self capture:@"PAD0_RIGHTSTICK_DOWN"]; return; }
	if ( rx < -threshold ) { [self capture:@"PAD0_RIGHTSTICK_LEFT"]; return; }
	if ( rx > threshold ) { [self capture:@"PAD0_RIGHTSTICK_RIGHT"]; return; }
	if ( @available(iOS 13.0, *) ) {
		if ( pad.buttonMenu.isPressed ) { [self capture:@"PAD0_START"]; return; }
		if ( pad.buttonOptions.isPressed ) { [self capture:@"PAD0_BACK"]; return; }
	}
}

@end

static NSArray<NSDictionary *> *IOS_GamepadAllActions( void )
{
	return @[
		@{@"section": @"Movement", @"command": @"+forward", @"label": @"Walk forward"},
		@{@"section": @"Movement", @"command": @"+back", @"label": @"Backpedal"},
		@{@"section": @"Movement", @"command": @"+moveleft", @"label": @"Step left"},
		@{@"section": @"Movement", @"command": @"+moveright", @"label": @"Step right"},
		@{@"section": @"Movement", @"command": @"+moveup", @"label": @"Jump"},
		@{@"section": @"Movement", @"command": @"+movedown", @"label": @"Crouch"},
		@{@"section": @"Looking", @"command": @"+left", @"label": @"Turn left"},
		@{@"section": @"Looking", @"command": @"+right", @"label": @"Turn right"},
		@{@"section": @"Looking", @"command": @"+lookup", @"label": @"Look up"},
		@{@"section": @"Looking", @"command": @"+lookdown", @"label": @"Look down"},
		@{@"section": @"Looking", @"command": @"centerview", @"label": @"Center view"},
		@{@"section": @"Weapons", @"command": @"+attack", @"label": @"Attack"},
		@{@"section": @"Weapons", @"command": @"+zoom", @"label": @"Zoom"},
		@{@"section": @"Weapons", @"command": @"weapnext", @"label": @"Next weapon"},
		@{@"section": @"Weapons", @"command": @"weapprev", @"label": @"Previous weapon"},
		@{@"section": @"Misc", @"command": @"+use", @"label": @"Use"},
		@{@"section": @"Misc", @"command": @"togglemenu", @"label": @"Toggle menu"}
	];
}

typedef NS_ENUM(NSInteger, SGTouchSettingRow) {
	SGTouchSettingRowAim = 0,
	SGTouchSettingRowMove,
	SGTouchSettingRowMoveHoriz,
	SGTouchSettingRowFire,
	SGTouchSettingRowEditLayout,
	SGTouchSettingRowSensitivity
};

@interface SGTouchSettingsViewController : UITableViewController
@end

@implementation SGTouchSettingsViewController

- (void)applyChrome {
	self.tableView.backgroundColor = [UIColor colorWithRed:0.08 green:0.07 blue:0.06 alpha:1.0];
	self.tableView.separatorColor = [UIColor colorWithWhite:1.0 alpha:0.12];
	self.navigationController.navigationBar.barTintColor = [UIColor colorWithRed:0.10 green:0.08 blue:0.07 alpha:1.0];
	self.navigationController.navigationBar.titleTextAttributes = @{
		NSForegroundColorAttributeName: [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:1.0]
	};
}

- (void)viewDidLoad {
	[super viewDidLoad];
	self.title = @"Touch Controls";
	self.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleGrouped];
	[self applyChrome];

	UIBarButtonItem *close = [[UIBarButtonItem alloc] initWithTitle:@"Done"
		style:UIBarButtonItemStyleDone target:self action:@selector(closeTapped)];
	close.tintColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0];
	self.navigationItem.rightBarButtonItem = close;
}

- (void)closeTapped {
	Cbuf_AddText( "writeconfig\n" );
	[self dismissViewControllerAnimated:YES completion:nil];
}

- (BOOL)touchMoveIsStick {
	return CL_GetCvarInt( "in_touchMoveMode" ) == 0;
}

- (NSInteger)touchSettingsRowCount {
	return [self touchMoveIsStick] ? 6 : 5;
}

- (SGTouchSettingRow)touchSettingRowAtIndex:(NSInteger)row {
	if ( [self touchMoveIsStick] ) {
		return (SGTouchSettingRow)row;
	}
	if ( row >= SGTouchSettingRowMoveHoriz )
		return (SGTouchSettingRow)( row + 1 );
	return (SGTouchSettingRow)row;
}

- (UITableViewCell *)touchSegmentCellWithTitle:(NSString *)title
	segments:(NSArray<NSString *> *)segments
	selectedIndex:(NSInteger)selectedIndex
	tag:(NSInteger)tag
{
	UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
	cell.selectionStyle = UITableViewCellSelectionStyleNone;
	cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];

	UILabel *label = [[UILabel alloc] init];
	label.text = title;
	label.textColor = UIColor.whiteColor;
	label.font = [UIFont systemFontOfSize:15 weight:UIFontWeightMedium];

	UISegmentedControl *segmented = [[UISegmentedControl alloc] initWithItems:segments];
	segmented.selectedSegmentIndex = selectedIndex;
	segmented.tag = tag;
	segmented.apportionsSegmentWidthsByContent = YES;
	if ( @available(iOS 13.0, *) ) {
		segmented.selectedSegmentTintColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0];
		segmented.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.08];
	}
	[segmented setTitleTextAttributes:@{ NSForegroundColorAttributeName: UIColor.whiteColor }
		forState:UIControlStateNormal];
	[segmented setTitleTextAttributes:@{ NSForegroundColorAttributeName: UIColor.blackColor }
		forState:UIControlStateSelected];
	[segmented addTarget:self action:@selector(touchSegmentChanged:) forControlEvents:UIControlEventValueChanged];

	UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[ label, segmented ]];
	stack.axis = UILayoutConstraintAxisVertical;
	stack.spacing = 10;
	stack.translatesAutoresizingMaskIntoConstraints = NO;
	[cell.contentView addSubview:stack];
	[NSLayoutConstraint activateConstraints:@[
		[stack.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:12],
		[stack.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor constant:16],
		[stack.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor constant:-16],
		[stack.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor constant:-12]
	]];
	return cell;
}

- (void)touchSegmentChanged:(UISegmentedControl *)sender {
	const char *value = sender.selectedSegmentIndex == 0 ? "0" : "1";
	switch ( sender.tag ) {
		case SGTouchSettingRowAim:
			Cvar_Set( "in_touchAimMode", value );
			break;
		case SGTouchSettingRowMove:
			Cvar_Set( "in_touchMoveMode", value );
			[self.tableView reloadData];
			break;
		case SGTouchSettingRowMoveHoriz:
			Cvar_Set( "in_touchMoveHoriz", value );
			break;
		case SGTouchSettingRowFire:
			Cvar_Set( "in_touchFireMode", value );
			break;
		default:
			break;
	}
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
	return 1;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
	(void)section;
	return @"Controls";
}

- (void)tableView:(UITableView *)tableView willDisplayHeaderView:(UIView *)view forSection:(NSInteger)section {
	(void)section;
	if ( [view isKindOfClass:[UITableViewHeaderFooterView class]] ) {
		UITableViewHeaderFooterView *header = (UITableViewHeaderFooterView *)view;
		header.textLabel.textColor = [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:0.85];
	}
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	(void)section;
	return [self touchSettingsRowCount];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	SGTouchSettingRow row = [self touchSettingRowAtIndex:indexPath.row];
	if ( row == SGTouchSettingRowAim ) {
		return [self touchSegmentCellWithTitle:@"Aim Control"
			segments:@[ @"Stick", @"Screen" ]
			selectedIndex:CL_GetCvarInt( "in_touchAimMode" ) == 0 ? 0 : 1
			tag:SGTouchSettingRowAim];
	}
	if ( row == SGTouchSettingRowMove ) {
		return [self touchSegmentCellWithTitle:@"Move Control"
			segments:@[ @"Stick", @"Screen" ]
			selectedIndex:CL_GetCvarInt( "in_touchMoveMode" ) == 0 ? 0 : 1
			tag:SGTouchSettingRowMove];
	}
	if ( row == SGTouchSettingRowMoveHoriz ) {
		return [self touchSegmentCellWithTitle:@"Move Horizontal Axis"
			segments:@[ @"Strafe", @"Turn" ]
			selectedIndex:CL_GetCvarInt( "in_touchMoveHoriz" ) == 0 ? 0 : 1
			tag:SGTouchSettingRowMoveHoriz];
	}
	if ( row == SGTouchSettingRowFire ) {
		return [self touchSegmentCellWithTitle:@"Fire Input"
			segments:@[ @"1/2 Tap", @"Buttons" ]
			selectedIndex:CL_GetCvarInt( "in_touchFireMode" ) == 0 ? 0 : 1
			tag:SGTouchSettingRowFire];
	}

	UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
	cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
	cell.textLabel.textColor = UIColor.whiteColor;
	cell.detailTextLabel.textColor = [UIColor colorWithWhite:1.0 alpha:0.55];
	if ( row == SGTouchSettingRowEditLayout ) {
		cell.textLabel.text = @"Edit Button Layout";
		cell.detailTextLabel.text = @"Move buttons and resize with the slider";
		cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
	} else {
		cell.textLabel.text = @"Touch Sensitivity";
		cell.detailTextLabel.text = @"Adjust in Setup > Controls";
		cell.selectionStyle = UITableViewCellSelectionStyleNone;
	}
	return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	[tableView deselectRowAtIndexPath:indexPath animated:YES];
	SGTouchSettingRow row = [self touchSettingRowAtIndex:indexPath.row];
	if ( row != SGTouchSettingRowEditLayout )
		return;

	Cbuf_AddText( "writeconfig\n" );
	__weak typeof(self) weakSelf = self;
	[self dismissViewControllerAnimated:YES completion:^{
		IN_TouchEnterEditMode();
		(void)weakSelf;
	}];
}

@end

@interface SGGamepadSettingsViewController : UITableViewController
@property (nonatomic, strong) SGGamepadCapture *capture;
@property (nonatomic, copy) NSString *waitingForCommand;
@property (nonatomic, strong) UILabel *captureBanner;
@end

@implementation SGGamepadSettingsViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	self.title = @"Gamepad";
		self.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleGrouped];
	self.tableView.backgroundColor = [UIColor colorWithRed:0.08 green:0.07 blue:0.06 alpha:1.0];
	self.tableView.separatorColor = [UIColor colorWithWhite:1.0 alpha:0.12];
	self.navigationController.navigationBar.prefersLargeTitles = NO;
	self.navigationController.navigationBar.barTintColor = [UIColor colorWithRed:0.10 green:0.08 blue:0.07 alpha:1.0];
	self.navigationController.navigationBar.titleTextAttributes = @{
		NSForegroundColorAttributeName: [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:1.0]
	};

	UIBarButtonItem *close = [[UIBarButtonItem alloc] initWithTitle:@"Done"
		style:UIBarButtonItemStyleDone target:self action:@selector(closeTapped)];
	close.tintColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0];
	self.navigationItem.rightBarButtonItem = close;

	_captureBanner = [[UILabel alloc] initWithFrame:CGRectZero];
	_captureBanner.translatesAutoresizingMaskIntoConstraints = NO;
	_captureBanner.textAlignment = NSTextAlignmentCenter;
	_captureBanner.numberOfLines = 0;
	_captureBanner.font = [UIFont boldSystemFontOfSize:14];
	_captureBanner.textColor = [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:1.0];
	_captureBanner.backgroundColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:0.15];
	_captureBanner.layer.cornerRadius = 8;
	_captureBanner.clipsToBounds = YES;
	_captureBanner.hidden = YES;
	[self.view addSubview:_captureBanner];
	[NSLayoutConstraint activateConstraints:@[
		[_captureBanner.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:8],
		[_captureBanner.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:16],
		[_captureBanner.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-16],
		[_captureBanner.heightAnchor constraintEqualToConstant:0]
	]];

	_capture = [[SGGamepadCapture alloc] init];
	__weak typeof(self) weakSelf = self;
	_capture.onInputCaptured = ^( NSString *input ) {
		if ( weakSelf.waitingForCommand.length == 0 ) return;
		[SGGamepadConfig.shared setBindingInput:input command:weakSelf.waitingForCommand];
		[weakSelf cancelCapture];
		[weakSelf.tableView reloadData];
	};
}

- (void)closeTapped {
	[self cancelCapture];
	[SGGamepadConfig.shared persist];
	Cbuf_AddText( "writeconfig\n" );
	[self dismissViewControllerAnimated:YES completion:^{
		[SGGamepadConfig.shared applyToRunningEngine];
	}];
}

- (void)viewWillDisappear:(BOOL)animated {
	[super viewWillDisappear:animated];
	[self cancelCapture];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
	return 6;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
	switch ( section ) {
		case 0: return @"Settings";
		case 1: return @"Movement";
		case 2: return @"Looking";
		case 3: return @"Weapons";
		case 4: return @"Misc";
		default: return @"Touch Controls";
	}
}

- (void)tableView:(UITableView *)tableView willDisplayHeaderView:(UIView *)view forSection:(NSInteger)section {
	if ( [view isKindOfClass:[UITableViewHeaderFooterView class]] ) {
		UITableViewHeaderFooterView *header = (UITableViewHeaderFooterView *)view;
		header.textLabel.textColor = [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:0.85];
	}
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	if ( section == 0 ) return 5;
	if ( section == 5 ) return 1;
	NSString *sectionName = @[@"", @"Movement", @"Looking", @"Weapons", @"Misc"][section];
	NSUInteger count = 0;
	for ( NSDictionary *action in IOS_GamepadAllActions() ) {
		if ( [action[@"section"] isEqualToString:sectionName] ) count++;
	}
	return (NSInteger)count;
}

- (NSArray<NSDictionary *> *)actionsForSection:(NSInteger)section {
	NSString *sectionName = @[@"", @"Movement", @"Looking", @"Weapons", @"Misc"][section];
	NSMutableArray *items = [NSMutableArray array];
	for ( NSDictionary *action in IOS_GamepadAllActions() ) {
		if ( [action[@"section"] isEqualToString:sectionName] ) {
			[items addObject:action];
		}
	}
	return items;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	if ( indexPath.section == 0 ) {
		if ( indexPath.row == 4 ) {
			UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
			cell.textLabel.text = @"Reset to Defaults";
			cell.textLabel.textColor = [UIColor colorWithRed:0.86 green:0.30 blue:0.22 alpha:1.0];
			cell.textLabel.textAlignment = NSTextAlignmentCenter;
			cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
			return cell;
		}
		if ( indexPath.row == 3 ) {
			UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
			cell.selectionStyle = UITableViewCellSelectionStyleNone;
			cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
			cell.textLabel.text = @"Invert Look (Y)";
			cell.textLabel.textColor = UIColor.whiteColor;
			UISwitch *toggle = [[UISwitch alloc] init];
			toggle.on = SGGamepadConfig.shared.lookInverted;
			toggle.onTintColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0];
			[toggle addTarget:self action:@selector(invertLookChanged:) forControlEvents:UIControlEventValueChanged];
			cell.accessoryView = toggle;
			return cell;
		}
		UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
		cell.selectionStyle = UITableViewCellSelectionStyleNone;
		cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];

		UILabel *title = [[UILabel alloc] init];
		if ( indexPath.row == 0 ) {
			title.text = @"Look Sensitivity";
		} else if ( indexPath.row == 1 ) {
			title.text = @"Stick Dead Zone";
		} else {
			title.text = @"Look Acceleration";
		}
		title.textColor = UIColor.whiteColor;
		UILabel *value = [[UILabel alloc] init];
		value.font = [UIFont monospacedDigitSystemFontOfSize:15 weight:UIFontWeightMedium];
		value.textColor = [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:1.0];
		value.tag = 3000 + (int)indexPath.row;
		UISlider *slider = [[UISlider alloc] init];
		slider.tag = (int)indexPath.row;
		slider.continuous = YES;
		slider.minimumTrackTintColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0];
		if ( indexPath.row == 0 ) {
			slider.minimumValue = 1;
			slider.maximumValue = 20;
			slider.value = SGGamepadConfig.shared.sensitivity;
			value.text = [NSString stringWithFormat:@"%.1f", slider.value];
		} else if ( indexPath.row == 1 ) {
			slider.minimumValue = 0.05f;
			slider.maximumValue = 0.5f;
			slider.value = SGGamepadConfig.shared.deadZone;
			value.text = [NSString stringWithFormat:@"%.2f", slider.value];
		} else {
			slider.minimumValue = 1.0f;
			slider.maximumValue = 3.0f;
			slider.value = SGGamepadConfig.shared.lookAcceleration;
			value.text = [NSString stringWithFormat:@"%.1f", slider.value];
		}
		[slider addTarget:self action:@selector(sliderChanged:) forControlEvents:UIControlEventValueChanged];
		[slider addTarget:self action:@selector(sliderFinished:) forControlEvents:UIControlEventTouchUpInside];
		[slider addTarget:self action:@selector(sliderFinished:) forControlEvents:UIControlEventTouchUpOutside];
		[slider addTarget:self action:@selector(sliderFinished:) forControlEvents:UIControlEventTouchCancel];

		UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[
			[[UIStackView alloc] initWithArrangedSubviews:@[title, value]],
			slider
		]];
		stack.axis = UILayoutConstraintAxisVertical;
		stack.spacing = 8;
		stack.translatesAutoresizingMaskIntoConstraints = NO;
		[cell.contentView addSubview:stack];
		[NSLayoutConstraint activateConstraints:@[
			[stack.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:12],
			[stack.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor constant:16],
			[stack.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor constant:-16],
			[stack.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor constant:-12]
		]];
		return cell;
	}

	if ( indexPath.section == 5 ) {
		UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
		cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
		cell.textLabel.text = @"Touch Controls";
		cell.textLabel.textColor = UIColor.whiteColor;
		cell.detailTextLabel.text = @"Aim, move, fire and button layout";
		cell.detailTextLabel.textColor = [UIColor colorWithWhite:1.0 alpha:0.55];
		cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
		return cell;
	}

	NSDictionary *action = [self actionsForSection:indexPath.section][indexPath.row];
	UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"bind"];
	if ( !cell ) {
		cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"bind"];
	}
	cell.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.06];
	cell.textLabel.text = action[@"label"];
	cell.textLabel.textColor = UIColor.whiteColor;
	cell.detailTextLabel.text = [SGGamepadConfig.shared displayNameForInput:
		[SGGamepadConfig.shared inputForCommand:action[@"command"]]];
	cell.detailTextLabel.textColor = [UIColor colorWithWhite:1.0 alpha:0.45];
	cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
	if ( [self.waitingForCommand isEqualToString:action[@"command"]] ) {
		cell.backgroundColor = [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:0.18];
	}
	return cell;
}

- (void)invertLookChanged:(UISwitch *)sender {
	SGGamepadConfig.shared.lookInverted = sender.isOn;
	[SGGamepadConfig.shared persist];
	Cvar_Set( "in_gamepadLookInvert", sender.isOn ? "1" : "0" );
}

- (UILabel *)valueLabelForSliderRow:(NSInteger)row {
	UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:row inSection:0]];
	if ( !cell ) return nil;
	return (UILabel *)[cell.contentView viewWithTag:3000 + (int)row];
}

- (void)updateValueLabelForSlider:(UISlider *)sender {
	UILabel *valueLabel = [self valueLabelForSliderRow:sender.tag];
	if ( !valueLabel ) return;

	if ( sender.tag == 0 ) {
		valueLabel.text = [NSString stringWithFormat:@"%.1f", sender.value];
	} else if ( sender.tag == 1 ) {
		valueLabel.text = [NSString stringWithFormat:@"%.2f", sender.value];
	} else {
		valueLabel.text = [NSString stringWithFormat:@"%.1f", sender.value];
	}
}

- (void)sliderChanged:(UISlider *)sender {
	if ( sender.tag == 0 ) {
		SGGamepadConfig.shared.sensitivity = sender.value;
	} else if ( sender.tag == 1 ) {
		SGGamepadConfig.shared.deadZone = sender.value;
	} else {
		SGGamepadConfig.shared.lookAcceleration = sender.value;
	}
	[self updateValueLabelForSlider:sender];
}

- (void)sliderFinished:(UISlider *)sender {
	[self sliderChanged:sender];
	[SGGamepadConfig.shared persist];
	if ( sender.tag == 2 ) {
		Cvar_Set( "in_gamepadLookAccel", [NSString stringWithFormat:@"%.1f", sender.value].UTF8String );
	}
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	[tableView deselectRowAtIndexPath:indexPath animated:YES];
	if ( indexPath.section == 0 && indexPath.row == 4 ) {
		UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Reset Gamepad"
			message:@"Restore default button and stick mappings?" preferredStyle:UIAlertControllerStyleAlert];
		[alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
		[alert addAction:[UIAlertAction actionWithTitle:@"Reset" style:UIAlertActionStyleDestructive handler:^( UIAlertAction *action ) {
			[self cancelCapture];
			[SGGamepadConfig.shared resetToDefaults];
			[self.tableView reloadData];
		}]];
		[self presentViewController:alert animated:YES completion:nil];
		return;
	}
	if ( indexPath.section == 5 ) {
		SGTouchSettingsViewController *touch = [[SGTouchSettingsViewController alloc] init];
		[touch applyChrome];
		[self.navigationController pushViewController:touch animated:YES];
		return;
	}
	if ( indexPath.section == 0 ) return;

	NSDictionary *action = [self actionsForSection:indexPath.section][indexPath.row];
	UIAlertController *sheet = [UIAlertController alertControllerWithTitle:action[@"label"]
		message:nil preferredStyle:UIAlertControllerStyleActionSheet];
	__weak typeof(self) weakSelf = self;
	[sheet addAction:[UIAlertAction actionWithTitle:@"Press to Bind" style:UIAlertActionStyleDefault handler:^( UIAlertAction *a ) {
		weakSelf.waitingForCommand = action[@"command"];
		[weakSelf.capture start];
		weakSelf.captureBanner.hidden = NO;
		weakSelf.captureBanner.text = [NSString stringWithFormat:@"Press a button or move a stick for \"%@\"", action[@"label"]];
		for ( NSLayoutConstraint *c in weakSelf.captureBanner.constraints ) {
			if ( c.firstAttribute == NSLayoutAttributeHeight ) c.constant = 44;
		}
		[weakSelf.tableView reloadData];
	}]];
	[sheet addAction:[UIAlertAction actionWithTitle:@"Clear Binding" style:UIAlertActionStyleDestructive handler:^( UIAlertAction *a ) {
		[SGGamepadConfig.shared clearBindingForCommand:action[@"command"]];
		[weakSelf.tableView reloadRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
	}]];
	[sheet addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^( UIAlertAction *a ) {
		[weakSelf cancelCapture];
	}]];
	[self presentViewController:sheet animated:YES completion:nil];
}

- (void)cancelCapture {
	self.waitingForCommand = nil;
	[_capture stop];
	_captureBanner.hidden = YES;
	for ( NSLayoutConstraint *c in _captureBanner.constraints ) {
		if ( c.firstAttribute == NSLayoutAttributeHeight ) c.constant = 0;
	}
	[self.tableView reloadData];
}

@end

static UIViewController *IOS_PresentRootViewController( void )
{
	UIViewController *root = UIApplication.sharedApplication.keyWindow.rootViewController;
	while ( root.presentedViewController ) {
		root = root.presentedViewController;
	}
	return root;
}

void IOS_Touch_PresentSettings( void )
{
	IOS_Gamepad_OnMain( ^{
		SGTouchSettingsViewController *settings = [[SGTouchSettingsViewController alloc] init];
		UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:settings];
		nav.modalPresentationStyle = UIModalPresentationFormSheet;
		[IOS_PresentRootViewController() presentViewController:nav animated:YES completion:nil];
	} );
}

void IOS_Gamepad_PresentSettings( void )
{
	IOS_Gamepad_OnMain( ^{
		SGGamepadSettingsViewController *settings = [[SGGamepadSettingsViewController alloc] init];
		UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:settings];
		nav.modalPresentationStyle = UIModalPresentationFormSheet;
		[IOS_PresentRootViewController() presentViewController:nav animated:YES completion:nil];
	} );
}
