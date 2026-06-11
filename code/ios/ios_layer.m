#include "ios_layer.h"
#include "ios_gamepad.h"
#include "../client/cl_touch.h"

#import <UIKit/UIKit.h>

static iosLayout_t iosLayout = { 0, 0, 0, 0, 0, 0, 1 };
static qboolean iosOverlayVisible = qtrue;
static volatile qboolean iosAppActive = qtrue;
static qboolean iosLifecycleObserversInstalled = qfalse;

typedef struct iosTouchOverlayState_s
{
	qboolean visible;
	float opacity;
	int mode;
	float moveX, moveY, moveRadius;
	qboolean moveActive;
	float lookX, lookY, lookRadius;
	qboolean lookActive;
	float fireX, fireY, fireRadius;
	qboolean fireActive;
	float altFireX, altFireY, altFireRadius;
	qboolean altFireActive;
	float jumpX, jumpY, jumpRadius;
	qboolean jumpActive;
	float crouchX, crouchY, crouchRadius;
	qboolean crouchActive;
	float useX, useY, useRadius;
	qboolean useActive;
	float openX, openY, openRadius;
	qboolean openActive;
	float weaponX, weaponY, weaponRadius;
	float menuX, menuY, menuRadius;
	float configX, configY, configRadius;
	qboolean configActive;
	qboolean editMode;
	qboolean gamepadMode;
	float sliderX, sliderY, sliderW;
	float sliderValue;
} iosTouchOverlayState_t;

static iosTouchOverlayState_t iosTouchOverlay;

@interface SGTouchOverlayView : UIView
@end

@interface SGTouchOverlayViewController : UIViewController
@end

static UIWindow *iosTouchWindow = nil;
static SGTouchOverlayView *iosTouchView = nil;

static void IOS_OnMainAsync( void (^block)( void ) )
{
	if( [NSThread isMainThread] )
		block();
	else
		dispatch_async( dispatch_get_main_queue(), block );
}

static void IOS_HideSystemChrome( void )
{
	IOS_OnMainAsync( ^{
		UIApplication *app = UIApplication.sharedApplication;
		if( [app respondsToSelector:@selector(setStatusBarHidden:withAnimation:)] )
			[app setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];

		for( UIWindow *window in app.windows )
		{
			[window.rootViewController setNeedsStatusBarAppearanceUpdate];
			if( [window.rootViewController respondsToSelector:@selector(setNeedsUpdateOfHomeIndicatorAutoHidden)] )
				[window.rootViewController setNeedsUpdateOfHomeIndicatorAutoHidden];
		}
	} );
}

void IOS_Layer_SetActive( qboolean active )
{
	iosAppActive = active;
}

qboolean IOS_Layer_IsActive( void )
{
	return iosAppActive;
}

static void IOS_InstallLifecycleObservers( void )
{
	if( iosLifecycleObserversInstalled )
		return;

	iosLifecycleObserversInstalled = qtrue;
	IOS_OnMainAsync( ^{
		NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
		NSOperationQueue *queue = NSOperationQueue.mainQueue;

		[center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qfalse );
			}];
		[center addObserverForName:UIApplicationDidEnterBackgroundNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qfalse );
			}];
		[center addObserverForName:UIApplicationWillEnterForegroundNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qtrue );
			}];
		[center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qtrue );
				IOS_HideSystemChrome();
			}];
	} );
}

static UIWindowScene *IOS_ActiveWindowScene( void )
{
	for( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if( [scene isKindOfClass:[UIWindowScene class]] &&
			scene.activationState == UISceneActivationStateForegroundActive )
			return (UIWindowScene *)scene;
	}

	for( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if( [scene isKindOfClass:[UIWindowScene class]] )
			return (UIWindowScene *)scene;
	}

	return nil;
}

static void IOS_UpdateLayoutFromView( UIView *view )
{
	CGRect bounds;
	CGFloat scale;

	if( !view )
		return;

	bounds = view.bounds;
	scale = view.window.screen.scale > 0.0 ? view.window.screen.scale : UIScreen.mainScreen.scale;
	iosLayout.width = (float)bounds.size.width;
	iosLayout.height = (float)bounds.size.height;
	iosLayout.scale = (float)scale;

	if( @available(iOS 11.0, *) )
	{
		UIEdgeInsets insets = view.safeAreaInsets;
		iosLayout.safeLeft = (float)insets.left;
		iosLayout.safeTop = (float)insets.top;
		iosLayout.safeRight = (float)insets.right;
		iosLayout.safeBottom = (float)insets.bottom;
	}
}

static void IOS_EnsureTouchOverlay( void )
{
	IOS_OnMainAsync( ^{
		UIWindowScene *scene;
		SGTouchOverlayViewController *vc;

		if( iosTouchWindow && iosTouchView )
		{
			IOS_HideSystemChrome();
			return;
		}

		scene = IOS_ActiveWindowScene();
		if( scene )
			iosTouchWindow = [[UIWindow alloc] initWithWindowScene:scene];
		else
			iosTouchWindow = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];

		iosTouchWindow.backgroundColor = UIColor.clearColor;
		iosTouchWindow.opaque = NO;
		iosTouchWindow.windowLevel = UIWindowLevelStatusBar + 2.0;

		vc = [[SGTouchOverlayViewController alloc] init];
		iosTouchView = [[SGTouchOverlayView alloc] initWithFrame:UIScreen.mainScreen.bounds];
		iosTouchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
		iosTouchView.backgroundColor = UIColor.clearColor;
		iosTouchView.opaque = NO;
		iosTouchView.multipleTouchEnabled = YES;
		iosTouchView.userInteractionEnabled = YES;
		vc.view = iosTouchView;
		iosTouchWindow.rootViewController = vc;
		[iosTouchWindow makeKeyAndVisible];
		IOS_UpdateLayoutFromView( iosTouchView );
		IOS_HideSystemChrome();
	} );
}

static CGFloat IOS_PointScale( void )
{
	return iosLayout.scale > 0.0f ? (CGFloat)iosLayout.scale : UIScreen.mainScreen.scale;
}

static CGRect IOS_RectFromPixelCenter( float x, float y, float radius, CGFloat factor )
{
	CGFloat scale = IOS_PointScale();
	CGFloat pr = (CGFloat)radius / scale;
	CGFloat px = (CGFloat)x / scale;
	CGFloat py = (CGFloat)y / scale;

	return CGRectMake( px - pr * factor, py - pr * factor, pr * factor * 2.0, pr * factor * 2.0 );
}

@implementation SGTouchOverlayViewController

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIStatusBarAnimation)preferredStatusBarUpdateAnimation { return UIStatusBarAnimationNone; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (BOOL)prefersPointerLocked { return YES; }

@end

@implementation SGTouchOverlayView

- (void)layoutSubviews
{
	[super layoutSubviews];
	IOS_UpdateLayoutFromView( self );
	IOS_HideSystemChrome();
}

- (void)drawRect:(CGRect)rect
{
	CGContextRef ctx = UIGraphicsGetCurrentContext();
	CGFloat scale = IOS_PointScale();
	CGFloat alpha;
	(void)rect;

	if( !ctx || !iosOverlayVisible || !iosTouchOverlay.visible )
		return;

	alpha = iosTouchOverlay.opacity;
	if( alpha < 0.05 )
		alpha = 0.05;
	if( alpha > 0.85 )
		alpha = 0.85;

	CGContextSetLineWidth( ctx, 7.0 );

	void (^drawButton)( float, float, float, NSString *, UIColor *, BOOL ) =
		^( float x, float y, float radius, NSString *label, UIColor *color, BOOL active ) {
			CGRect buttonRect = IOS_RectFromPixelCenter( x, y, radius, 0.58 );
			CGRect borderRect = IOS_RectFromPixelCenter( x, y, radius, 1.0 );
			CGFloat a = active ? MIN( alpha + 0.22, 0.92 ) : alpha;
			[color colorWithAlphaComponent:a];
			CGContextSetFillColorWithColor( ctx, [color colorWithAlphaComponent:a].CGColor );
			CGContextFillEllipseInRect( ctx, buttonRect );
			CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:(active ? 0.72 : 0.42)].CGColor );
			CGContextStrokeEllipseInRect( ctx, borderRect );

			NSDictionary *attrs = @{
				NSFontAttributeName: [UIFont boldSystemFontOfSize:MAX( 9.0, radius / scale * 0.18 )],
				NSForegroundColorAttributeName: [UIColor colorWithWhite:1.0 alpha:0.62]
			};
			CGSize sz = [label sizeWithAttributes:attrs];
			[label drawAtPoint:CGPointMake( (CGFloat)x / scale - sz.width * 0.5,
				(CGFloat)y / scale - sz.height * 0.5 ) withAttributes:attrs];
		};

	void (^drawStick)( float, float, float, BOOL ) =
		^( float x, float y, float radius, BOOL active ) {
			if( radius <= 0.0f )
				return;
			CGRect borderRect = IOS_RectFromPixelCenter( x, y, radius, 1.0 );
			CGRect knobRect = IOS_RectFromPixelCenter( x, y, radius * 0.35f, 1.0 );
			CGFloat a = active ? MIN( alpha + 0.18, 0.88 ) : alpha * 0.75f;
			CGContextSetFillColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:a * 0.18].CGColor );
			CGContextFillEllipseInRect( ctx, borderRect );
			CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:(active ? 0.72 : 0.42)].CGColor );
			CGContextStrokeEllipseInRect( ctx, borderRect );
			if( active )
			{
				CGContextSetFillColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.55].CGColor );
				CGContextFillEllipseInRect( ctx, knobRect );
			}
		};

	if( iosTouchOverlay.gamepadMode )
	{
		drawButton( iosTouchOverlay.configX, iosTouchOverlay.configY, iosTouchOverlay.configRadius,
			@"PAD", [UIColor colorWithRed:0.55 green:0.62 blue:0.70 alpha:1.0],
			iosTouchOverlay.configActive );
		return;
	}

	drawStick( iosTouchOverlay.moveX, iosTouchOverlay.moveY, iosTouchOverlay.moveRadius,
		iosTouchOverlay.moveActive );
	drawStick( iosTouchOverlay.lookX, iosTouchOverlay.lookY, iosTouchOverlay.lookRadius,
		iosTouchOverlay.lookActive );

	if( iosTouchOverlay.fireRadius > 0.0f )
		drawButton( iosTouchOverlay.fireX, iosTouchOverlay.fireY, iosTouchOverlay.fireRadius,
			@"FIRE", [UIColor colorWithRed:0.86 green:0.30 blue:0.22 alpha:1.0], iosTouchOverlay.fireActive );
	if( iosTouchOverlay.altFireRadius > 0.0f )
		drawButton( iosTouchOverlay.altFireX, iosTouchOverlay.altFireY, iosTouchOverlay.altFireRadius,
			@"ALT", [UIColor colorWithRed:0.76 green:0.28 blue:0.20 alpha:1.0], iosTouchOverlay.altFireActive );

	drawButton( iosTouchOverlay.jumpX, iosTouchOverlay.jumpY, iosTouchOverlay.jumpRadius,
		@"JUMP", [UIColor colorWithRed:0.16 green:0.70 blue:0.42 alpha:1.0], iosTouchOverlay.jumpActive );
	drawButton( iosTouchOverlay.crouchX, iosTouchOverlay.crouchY, iosTouchOverlay.crouchRadius,
		@"CRCH", [UIColor colorWithRed:0.12 green:0.50 blue:0.74 alpha:1.0], iosTouchOverlay.crouchActive );
	drawButton( iosTouchOverlay.useX, iosTouchOverlay.useY, iosTouchOverlay.useRadius,
		@"USE", [UIColor colorWithRed:0.88 green:0.54 blue:0.16 alpha:1.0], iosTouchOverlay.useActive );
	drawButton( iosTouchOverlay.openX, iosTouchOverlay.openY, iosTouchOverlay.openRadius,
		@"ZOOM", [UIColor colorWithRed:0.76 green:0.46 blue:0.20 alpha:1.0], iosTouchOverlay.openActive );
	drawButton( iosTouchOverlay.weaponX, iosTouchOverlay.weaponY, iosTouchOverlay.weaponRadius,
		@"WPN", [UIColor colorWithRed:0.90 green:0.72 blue:0.18 alpha:1.0], iosTouchOverlay.mode == 1 );
	drawButton( iosTouchOverlay.menuX, iosTouchOverlay.menuY, iosTouchOverlay.menuRadius,
		@"MENU", [UIColor colorWithWhite:0.66 alpha:1.0], NO );
	drawButton( iosTouchOverlay.configX, iosTouchOverlay.configY, iosTouchOverlay.configRadius,
		@"CFG", [UIColor colorWithRed:0.55 green:0.62 blue:0.70 alpha:1.0],
		iosTouchOverlay.configActive || iosTouchOverlay.editMode );

	if( iosTouchOverlay.editMode )
	{
		CGFloat sx = (CGFloat)iosTouchOverlay.sliderX / scale;
		CGFloat sy = (CGFloat)iosTouchOverlay.sliderY / scale;
		CGFloat sw = (CGFloat)iosTouchOverlay.sliderW / scale;
		CGFloat knob = sx + sw * iosTouchOverlay.sliderValue;
		CGRect rail = CGRectMake( sx, sy - 3.0, sw, 6.0 );
		CGRect fill = CGRectMake( sx, sy - 3.0, MAX( 0.0, knob - sx ), 6.0 );
		CGRect knobRect = CGRectMake( knob - 16.0, sy - 16.0, 32.0, 32.0 );

		CGContextSetFillColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.20].CGColor );
		CGContextFillRect( ctx, rail );
		CGContextSetFillColorWithColor( ctx, [UIColor colorWithRed:0.30 green:0.72 blue:0.86 alpha:0.62].CGColor );
		CGContextFillRect( ctx, fill );
		CGContextSetFillColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.72].CGColor );
		CGContextFillEllipseInRect( ctx, knobRect );
		CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:0.0 alpha:0.35].CGColor );
		CGContextStrokeEllipseInRect( ctx, knobRect );
	}
}

- (void)forwardTouches:(NSSet<UITouch *> *)touches down:(BOOL)down motion:(BOOL)motion
{
	CGRect bounds = self.bounds;
	for( UITouch *touch in touches )
	{
		CGPoint p = [touch locationInView:self];
		float nx = bounds.size.width > 0.0 ? (float)( p.x / bounds.size.width ) : 0.0f;
		float ny = bounds.size.height > 0.0 ? (float)( p.y / bounds.size.height ) : 0.0f;
		IN_TouchFinger( (long long)(uintptr_t)touch, nx, ny, down ? qtrue : qfalse, motion ? qtrue : qfalse );
	}
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:YES motion:NO];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:YES motion:YES];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:NO motion:NO];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:NO motion:NO];
}

@end

static void IOS_UpdateSafeArea( void )
{
	UIWindow *window = iosTouchWindow ? iosTouchWindow : UIApplication.sharedApplication.keyWindow;
	if( window && @available(iOS 11.0, *) )
	{
		UIEdgeInsets insets = window.safeAreaInsets;
		iosLayout.safeLeft = (float)insets.left;
		iosLayout.safeTop = (float)insets.top;
		iosLayout.safeRight = (float)insets.right;
		iosLayout.safeBottom = (float)insets.bottom;
	}
}

void IOS_Layer_Init( void )
{
	UIScreen *screen = UIScreen.mainScreen;
	CGSize size = screen.bounds.size;
	iosLayout.width = (float)size.width;
	iosLayout.height = (float)size.height;
	iosLayout.scale = (float)screen.scale;
	IOS_InstallLifecycleObservers();
	IOS_EnsureTouchOverlay();
	IOS_HideSystemChrome();
	IOS_UpdateSafeArea();
}

void IOS_Layer_Shutdown( void )
{
	IOS_OnMainAsync( ^{
		iosTouchWindow.hidden = YES;
		iosTouchWindow.rootViewController = nil;
		iosTouchWindow = nil;
		iosTouchView = nil;
	} );
}

void IOS_Layer_Tick( void )
{
	IOS_EnsureTouchOverlay();
	IOS_HideSystemChrome();
	IOS_UpdateSafeArea();
}

void IOS_Layer_SyncScreen( int width, int height, float scale )
{
	CGSize screenSize = UIScreen.mainScreen.bounds.size;
	float newScale = scale > 0 ? scale : iosLayout.scale;
	float drawableW = width > 0 ? (float)width / ( newScale > 0 ? newScale : 1.0f ) : 0.0f;
	float drawableH = height > 0 ? (float)height / ( newScale > 0 ? newScale : 1.0f ) : 0.0f;

	iosLayout.width = fmaxf( (float)screenSize.width, drawableW );
	iosLayout.height = fmaxf( (float)screenSize.height, drawableH );
	iosLayout.scale = newScale > 0 ? newScale : iosLayout.scale;
	IOS_EnsureTouchOverlay();
	IOS_UpdateSafeArea();
}

void IOS_Layer_GetLayout( iosLayout_t *layout )
{
	if( layout )
		*layout = iosLayout;
}

void IOS_Layer_SetGameOverlayVisible( qboolean visible )
{
	iosOverlayVisible = visible;
	IOS_OnMainAsync( ^{
		iosTouchView.hidden = NO;
		iosTouchView.userInteractionEnabled = YES;
		[iosTouchView setNeedsDisplay];
	} );
}

void IOS_Layer_UpdateTouchControls( qboolean visible, float opacity, int mode,
	float moveX, float moveY, float moveRadius, qboolean moveActive,
	float lookX, float lookY, float lookRadius, qboolean lookActive,
	float fireX, float fireY, float fireRadius, qboolean fireActive,
	float altFireX, float altFireY, float altFireRadius, qboolean altFireActive,
	float jumpX, float jumpY, float jumpRadius, qboolean jumpActive,
	float crouchX, float crouchY, float crouchRadius, qboolean crouchActive,
	float useX, float useY, float useRadius, qboolean useActive,
	float openX, float openY, float openRadius, qboolean openActive,
	float weaponX, float weaponY, float weaponRadius,
	float menuX, float menuY, float menuRadius,
	float configX, float configY, float configRadius, qboolean configActive,
	qboolean editMode, float sliderX, float sliderY, float sliderW,
	float sliderValue )
{
	iosTouchOverlay.visible = visible;
	iosTouchOverlay.opacity = opacity;
	iosTouchOverlay.mode = mode;
	iosTouchOverlay.moveX = moveX;
	iosTouchOverlay.moveY = moveY;
	iosTouchOverlay.moveRadius = moveRadius;
	iosTouchOverlay.moveActive = moveActive;
	iosTouchOverlay.lookX = lookX;
	iosTouchOverlay.lookY = lookY;
	iosTouchOverlay.lookRadius = lookRadius;
	iosTouchOverlay.lookActive = lookActive;
	iosTouchOverlay.fireX = fireX;
	iosTouchOverlay.fireY = fireY;
	iosTouchOverlay.fireRadius = fireRadius;
	iosTouchOverlay.fireActive = fireActive;
	iosTouchOverlay.altFireX = altFireX;
	iosTouchOverlay.altFireY = altFireY;
	iosTouchOverlay.altFireRadius = altFireRadius;
	iosTouchOverlay.altFireActive = altFireActive;
	iosTouchOverlay.jumpX = jumpX;
	iosTouchOverlay.jumpY = jumpY;
	iosTouchOverlay.jumpRadius = jumpRadius;
	iosTouchOverlay.jumpActive = jumpActive;
	iosTouchOverlay.crouchX = crouchX;
	iosTouchOverlay.crouchY = crouchY;
	iosTouchOverlay.crouchRadius = crouchRadius;
	iosTouchOverlay.crouchActive = crouchActive;
	iosTouchOverlay.useX = useX;
	iosTouchOverlay.useY = useY;
	iosTouchOverlay.useRadius = useRadius;
	iosTouchOverlay.useActive = useActive;
	iosTouchOverlay.openX = openX;
	iosTouchOverlay.openY = openY;
	iosTouchOverlay.openRadius = openRadius;
	iosTouchOverlay.openActive = openActive;
	iosTouchOverlay.weaponX = weaponX;
	iosTouchOverlay.weaponY = weaponY;
	iosTouchOverlay.weaponRadius = weaponRadius;
	iosTouchOverlay.menuX = menuX;
	iosTouchOverlay.menuY = menuY;
	iosTouchOverlay.menuRadius = menuRadius;
	iosTouchOverlay.configX = configX;
	iosTouchOverlay.configY = configY;
	iosTouchOverlay.configRadius = configRadius;
	iosTouchOverlay.configActive = configActive;
	iosTouchOverlay.editMode = editMode;
	iosTouchOverlay.sliderX = sliderX;
	iosTouchOverlay.sliderY = sliderY;
	iosTouchOverlay.sliderW = sliderW;
	if( sliderValue < 0.0f )
		sliderValue = 0.0f;
	if( sliderValue > 1.0f )
		sliderValue = 1.0f;
	iosTouchOverlay.sliderValue = sliderValue;

	IOS_EnsureTouchOverlay();
	IOS_OnMainAsync( ^{
		iosTouchView.hidden = NO;
		iosTouchView.userInteractionEnabled = YES;
		[iosTouchView setNeedsDisplay];
	} );
}

void IOS_Layer_HideTouchControls( int mode )
{
	IOS_Layer_UpdateTouchControls( qfalse, 0.0f, mode,
		0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse, 0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse,
		0, 0, 0,
		0, 0, 0,
		0, 0, 0, qfalse,
		qfalse, 0, 0, 0, 0 );
}

void IOS_Layer_UpdateTouchControlsGamepadOnly( float opacity, int mode,
	float configX, float configY, float configRadius, qboolean configActive )
{
	IOS_Layer_UpdateTouchControls( qtrue, opacity, mode,
		0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse, 0, 0, 0, qfalse, 0, 0, 0, qfalse,
		0, 0, 0, qfalse,
		0, 0, 0,
		0, 0, 0,
		configX, configY, configRadius, configActive,
		qfalse, 0, 0, 0, 0 );
}

void IOS_Layer_OpenTouchSettings( void )
{
	IOS_Touch_PresentSettings();
}

void IOS_Layer_SetTouchGamepadMode( qboolean gamepadMode )
{
	iosTouchOverlay.gamepadMode = gamepadMode;
	IOS_OnMainAsync( ^{
		[iosTouchView setNeedsDisplay];
	} );
}

void IOS_Layer_AttachToWindow( void )
{
	IOS_EnsureTouchOverlay();
	IOS_HideSystemChrome();
	IOS_UpdateSafeArea();
}
